#include "elf.h"
#include "dragonstub/linux-efi.h"
#include "dragonstub/linux/align.h"
#include "dragonstub/printk.h"
#include "dragonstub/riscv64.h"
#include "dragonstub/types.h"
#include "efidef.h"
#include <efi.h>
#include <efiapi.h>
#include <efidevp.h>
#include <efilib.h>
#include <dragonstub/dragonstub.h>
#include <dragonstub/elfloader.h>

/// @brief 校验ELF文件头
/// @param buf 缓冲区
/// @param bufsize 缓冲区大小
/// @return
static bool verify_ident(const void *buf, u64 bufsize)
{
	if (bufsize < EI_NIDENT) {
		// 太短，不是ELF
		return false;
	}
	// 检查magic number
	for (int i = 0; i < EI_CLASS; i++) {
		u8 c = *(u8 *)(buf + i);
		if (c != ELFMAG[i]) {
			// 不是ELF magic number，跳过
			efi_err("ELF magic number not match\n");
			return false;
		}
	}

	// verify ELF Version
	u8 version = *(u8 *)(buf + EI_VERSION);
	if (version != EV_CURRENT) {
		efi_err("ELF version not match, expected EV_CURRENT(%d), got %d\n",
			EV_CURRENT, version);
		// 不是当前版本，跳过
		return false;
	}

	// verify ELF Class
	u8 class = *(u8 *)(buf + EI_CLASS);
	if (class != ELFCLASS64) {
		efi_err("ELF class not match, expected ELFCLASS64(%d), got %d\n",
			ELFCLASS64, class);
		// 不是64位，跳过
		return false;
	}

	return true;
}

bool elf_check(const void *payload_start, u64 payload_size)
{
	// 校验ELF文件头
	if (!verify_ident(payload_start, payload_size)) {
		return false;
	}
	// 检查架构
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)payload_start;
#ifdef CONFIG_riscv64
	if (ehdr->e_machine != EM_RISCV) {
		efi_err("ELF machine not match, expected EM_RISCV(%d), got %d\n",
			EM_RISCV, ehdr->e_machine);
		return false;
	}
#else
// 还没有对当前架构进行检查，抛出编译错误
#error "Unimplement ELF arch test for current cross compile arch"
#endif
	return true;
}

/// @brief 获取ELF文件头
/// @param payload_start 文件起始地址
/// @param payload_size 文件大小
/// @param ehdr 返回的ELF文件头
/// @return
efi_status_t elf_get_header(const void *payload_start, u64 payload_size,
			    Elf64_Ehdr **ehdr)
{
	if (!verify_ident(payload_start, payload_size)) {
		return EFI_INVALID_PARAMETER;
	}
	*ehdr = (Elf64_Ehdr *)payload_start;
	return EFI_SUCCESS;
}

static void print_elf_info(Elf64_Ehdr *ehdr)
{
	efi_info("ELF header:\n");
	efi_printk("  e_type: %d\n", ehdr->e_type);
	efi_printk("  e_machine: %d\n", ehdr->e_machine);
	efi_printk("  e_version: %d\n", ehdr->e_version);
	efi_printk("  e_entry: %p\n", ehdr->e_entry);
	efi_printk("  e_phoff: %p\n", ehdr->e_phoff);
	efi_printk("  e_shoff: %p\n", ehdr->e_shoff);
	efi_printk("  e_flags: %d\n", ehdr->e_flags);
	efi_printk("  e_ehsize: %d\n", ehdr->e_ehsize);
	efi_printk("  e_phentsize: %d\n", ehdr->e_phentsize);
	efi_printk("  e_phnum: %d\n", ehdr->e_phnum);
	efi_printk("  e_shentsize: %d\n", ehdr->e_shentsize);
	efi_printk("  e_shnum: %d\n", ehdr->e_shnum);
	efi_printk("  e_shstrndx: %d\n", ehdr->e_shstrndx);
}

static efi_status_t parse_phdrs(const void *payload_start, u64 payload_size,
				const Elf64_Ehdr *ehdr, u32 *ret_segments_nr,
				Elf64_Phdr **ret_phdr)
{
	if (ehdr->e_phnum == 0) {
		efi_err("No program header\n");
		return EFI_INVALID_PARAMETER;
	}
	if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
		efi_err("Invalid program header size: %d, expected %d\n",
			ehdr->e_phentsize, sizeof(Elf64_Phdr));
		return EFI_INVALID_PARAMETER;
	}

	u16 phnum = ehdr->e_phnum;
	if (phnum == PN_XNUM) {
		u64 shoff = ehdr->e_shoff;
		if (shoff == 0) {
			efi_err("No section header\n");
			return EFI_INVALID_PARAMETER;
		}

		if (shoff + sizeof(Elf64_Shdr) > payload_size) {
			efi_err("Section header out of range\n");
			return EFI_INVALID_PARAMETER;
		}

		Elf64_Shdr *shdr = (Elf64_Shdr *)(payload_start + shoff);

		phnum = shdr[0].sh_info;
		if (phnum == 0) {
			efi_err("shdr[0].sh_info indicates no program header\n");
			return EFI_INVALID_PARAMETER;
		}
	}

	size_t phoff = ehdr->e_phoff;
	size_t phsize = ehdr->e_phentsize;
	size_t total_size = phnum * phsize;
	if (phoff + total_size > payload_size) {
		efi_err("Program header out of range\n");
		return EFI_INVALID_PARAMETER;
	}

	Elf64_Phdr *phdr = (Elf64_Phdr *)(payload_start + phoff);

	*ret_segments_nr = phnum;
	*ret_phdr = phdr;

	return EFI_SUCCESS;
}

/*
 * Distro versions of GRUB may ignore the BSS allocation entirely (i.e., fail
 * to provide space, and fail to zero it). Check for this condition by double
 * checking that the first and the last byte of the image are covered by the
 * same EFI memory map entry.
 */
static bool check_image_region(u64 base, u64 size)
{
	struct efi_boot_memmap *map;
	efi_status_t status;
	bool ret = false;
	u64 map_offset;

	status = efi_get_memory_map(&map, false);
	if (status != EFI_SUCCESS)
		return false;

	for (map_offset = 0; map_offset < map->map_size;
	     map_offset += map->desc_size) {
		efi_memory_desc_t *md = (void *)map->map + map_offset;
		u64 end = md->PhysicalStart + md->NumberOfPages * EFI_PAGE_SIZE;

		/*
		 * Find the region that covers base, and return whether
		 * it covers base+size bytes.
		 */
		if (base >= md->PhysicalStart && base < end) {
			ret = (base + size) <= end;
			break;
		}
	}

	efi_bs_call(FreePool, map);

	return ret;
}

/**
 * efi_remap_image_all_rwx - Remap a loaded image with the appropriate permissions
 *                   for code and data
 *
 * @image_base:	the base of the image in memory
 * @alloc_size:	the size of the area in memory occupied by the image
 *
 * efi_remap_image() uses the EFI memory attribute protocol to remap the code
 * region of the loaded image read-only/executable, and the remainder
 * read-write/non-executable. The code region is assumed to start at the base
 * of the image, and will therefore cover the PE/COFF header as well.
 */
void efi_remap_image_all_rwx(unsigned long image_base, unsigned alloc_size)
{
	efi_guid_t guid = EFI_MEMORY_ATTRIBUTE_PROTOCOL_GUID;
	efi_memory_attribute_protocol_t *memattr;
	efi_status_t status;
	u64 attr;

	/*
	 * If the firmware implements the EFI_MEMORY_ATTRIBUTE_PROTOCOL, let's
	 * invoke it to remap the text/rodata region of the decompressed image
	 * as read-only and the data/bss region as non-executable.
	 */
	status = efi_bs_call(LocateProtocol, &guid, NULL, (void **)&memattr);
	if (status != EFI_SUCCESS)
		return;

	// Get the current attributes for the entire region
	status = memattr->get_memory_attributes(memattr, image_base, alloc_size,
						&attr);
	if (status != EFI_SUCCESS) {
		efi_warn(
			"Failed to retrieve memory attributes for image region: 0x%lx\n",
			status);
		return;
	}

	efi_debug("Current attributes for image region: 0x%lx\n", attr);

	// If the entire region was already mapped as non-exec, clear the
	// attribute from the code region. Otherwise, set it on the data
	// region.
	if (attr & EFI_MEMORY_XP) {
		status = memattr->clear_memory_attributes(
			memattr, image_base, alloc_size, EFI_MEMORY_XP);
		if (status != EFI_SUCCESS)
			efi_warn("Failed to remap region executable\n");
	}

	if (attr & EFI_MEMORY_WP) {
		status = memattr->clear_memory_attributes(
			memattr, image_base, alloc_size, EFI_MEMORY_WP);
		if (status != EFI_SUCCESS)
			efi_warn("Failed to remap region writable\n");
	}

	if (attr & EFI_MEMORY_RP) {
		status = memattr->clear_memory_attributes(
			memattr, image_base, alloc_size, EFI_MEMORY_RP);
		if (status != EFI_SUCCESS)
			efi_warn("Failed to remap region readable\n");
	}
}

efi_status_t efi_allocate_kernel_memory(const Elf64_Phdr *phdr_start,
					u32 phdrs_nr, u64 *ret_paddr,
					u64 *ret_size, u64 *ret_min_paddr,
					u64 *ret_max_paddr, u64 *ret_min_vaddr)
{
	efi_status_t status = EFI_SUCCESS;
	const u64 KERNEL_MEM_ALIGN = 1 << 21; // 2MB

	const Elf64_Phdr *phdr = phdr_start;

	u64 min_paddr = UINT64_MAX;
	u64 max_paddr = 0;
	u64 min_vaddr = UINT64_MAX;

	for (u32 i = 0; i < phdrs_nr; ++i, ++phdr) {
		if (phdr->p_type != PT_LOAD) {
			continue;
		}

		if (phdr->p_align & !EFI_PAGE_SIZE) {
			efi_err("ELF segment alignment should be multiple of EFI_PAGE_SIZE(%d), but got %d\n",
				EFI_PAGE_SIZE, phdr->p_align);
			return EFI_INVALID_PARAMETER;
		}
		min_paddr = min(min_paddr, (u64)phdr->p_paddr);
		min_vaddr = min(min_vaddr, (u64)phdr->p_vaddr);
		max_paddr =
			max(max_paddr, (u64)(phdr->p_paddr + phdr->p_memsz));
	}

	if (min_paddr & (KERNEL_MEM_ALIGN - 1)) {
		efi_err("min_paddr should be aligned to KERNEL_MEM_ALIGN(%d), but got %p\n",
			KERNEL_MEM_ALIGN, min_paddr);
		return EFI_INVALID_PARAMETER;
	}
	u64 mem_size = ALIGN_UP(max_paddr - min_paddr, KERNEL_MEM_ALIGN);

	status = efi_allocate_pages_aligned(mem_size, ret_paddr, UINT64_MAX,
					    KERNEL_MEM_ALIGN, EfiLoaderData);
	// status = efi_allocate_pages_exact(mem_size, paddr);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate pages for ELF segment: status: %d, page_size=%d, min_paddr=%p, max_paddr=%p, mem_size=%d. Maybe an OOM error or section overlaps.\n",
			status, KERNEL_MEM_ALIGN, ret_paddr, max_paddr,
			mem_size);
		return status;
	}

	*ret_size = mem_size;
	*ret_min_paddr = min_paddr;
	*ret_max_paddr = max_paddr;
	*ret_min_vaddr = min_vaddr;
	efi_info("Allocated kernel memory: paddr=%p, mem_size= %d bytes\n",
		 *ret_paddr, mem_size);
	// zeroed the memory
	memset((void *)(*ret_paddr), 0, mem_size);

	efi_remap_image_all_rwx(*ret_paddr, mem_size);

	return EFI_SUCCESS;
}

static efi_status_t load_program(const void *payload_start, u64 payload_size,
				 const Elf64_Phdr *phdr_start, u32 phdrs_nr,
				 u64 *ret_program_mem_paddr,
				 u64 *ret_program_mem_size, u64 *ret_min_paddr,
				 u64 *ret_min_vaddr)
{
	efi_status_t status = EFI_SUCCESS;

	u64 allocated_paddr = 0;
	u64 allocated_size = 0;
	u64 min_paddr = 0;
	u64 max_paddr = 0;
	u64 min_vaddr = 0;
	status = efi_allocate_kernel_memory(phdr_start, phdrs_nr,
					    &allocated_paddr, &allocated_size,
					    &min_paddr, &max_paddr, &min_vaddr);

	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate kernel memory\n");
		return status;
	}

	// 清空内存
	memset((void *)allocated_paddr, 0, allocated_size);

	const Elf64_Phdr *phdr = phdr_start;

	for (u32 i = 0; i < phdrs_nr; ++i, ++phdr) {
		if (phdr->p_type != PT_LOAD) {
			continue;
		}

		if (phdr->p_align & !EFI_PAGE_SIZE) {
			efi_err("ELF segment alignment should be multiple of EFI_PAGE_SIZE(%d), but got %d\n",
				EFI_PAGE_SIZE, phdr->p_align);
			status = EFI_INVALID_PARAMETER;
			goto failed;
		}

		u64 paddr = phdr->p_paddr;

		u64 mem_size = phdr->p_memsz;
		u64 file_size = phdr->p_filesz;
		u64 file_offset = phdr->p_offset;
		// efi_debug(
		// 	"loading segment: paddr=%p, mem_size=%d, file_size=%d\n",
		// 	paddr, mem_size, file_size);

		if (file_offset + file_size > payload_size) {
			status = EFI_INVALID_PARAMETER;
			goto failed;
		}

		if (mem_size < file_size) {
			status = EFI_INVALID_PARAMETER;
			goto failed;
		}

		if (mem_size == 0) {
			continue;
		}

		memcpy((void *)(allocated_paddr + (paddr - min_paddr)),
		       payload_start + file_offset, file_size);

		// efi_debug(
		// 	"segment loaded: file_offset: %p paddr=%p, mem_size=%p, file_size=%p\n",
		// 	file_offset, paddr, mem_size, file_size);
	}

	*ret_program_mem_paddr = allocated_paddr;
	*ret_program_mem_size = allocated_size;
	*ret_min_paddr = min_paddr;
	*ret_min_vaddr = min_vaddr;

	return EFI_SUCCESS;
failed:
	efi_free(allocated_size, allocated_paddr);
	return status;
}

efi_status_t load_elf(struct payload_info *payload_info)
{
	const void *payload_start = (void *)payload_info->payload_addr;
	u64 payload_size = payload_info->payload_size;
	Elf64_Ehdr *ehdr = NULL;
	efi_status_t status =
		elf_get_header(payload_start, payload_size, &ehdr);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to get ELF header\n");
		return status;
	}
	ASSERT(ehdr != NULL);

	print_elf_info(ehdr);

	u32 phdrs_nr = 0;
	Elf64_Phdr *phdr_start = NULL;

	status = parse_phdrs(payload_start, payload_size, ehdr, &phdrs_nr,
			     &phdr_start);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to parse ELF segments\n");
		return status;
	}

	efi_debug("program headers: %d\n", phdrs_nr);

	u64 program_paddr = 0;
	u64 program_size = 0;
	u64 image_link_base_paddr = 0;
	u64 image_link_base_vaddr = 0;
	load_program(payload_start, payload_size, phdr_start, phdrs_nr,
		     &program_paddr, &program_size, &image_link_base_paddr,
		     &image_link_base_vaddr);
	payload_info->loaded_paddr = program_paddr;
	payload_info->loaded_size = program_size;
	payload_info->kernel_entry =
		ehdr->e_entry - image_link_base_vaddr + program_paddr;

	efi_info("loaded_paddr: %p\n", payload_info->loaded_paddr);
	efi_info("loaded_size: %p\n", payload_info->loaded_size);
	efi_info("ehdr->e_entry: %lx\n", ehdr->e_entry);
	efi_info("image_link_base_paddr: %lx\n", image_link_base_paddr);
	efi_info("kernel_entry: %lx\n", payload_info->kernel_entry);
	// 处理权限问题

	efi_remap_image_all_rwx(program_paddr, program_size);
	extern void _start(void);
	extern void _image_end(void);
	u64 image_size = (u64)&_image_end - (u64)&_start;
	efi_debug("image_size: %d\n", image_size);
	efi_remap_image_all_rwx((u64)&_start, (image_size + 4095) & ~4095);

	// 添加地址到efi configuration table

	struct dragonstub_payload_efi *tbl = NULL;
	status = efi_bs_call(AllocatePool, EfiLoaderData,
			     sizeof(struct dragonstub_payload_efi),
			     (void **)&tbl);

	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate memory for dragonstub_payload_efi\n");
		return status;
	}

	tbl->loaded_addr = payload_info->loaded_paddr;
	tbl->size = payload_info->loaded_size;

	efi_guid_t dragonstub_payload_efi_guid =
		DRAGONSTUB_EFI_PAYLOAD_EFI_GUID;

	status = efi_bs_call(InstallConfigurationTable,
			     &dragonstub_payload_efi_guid, tbl);

	if (status != EFI_SUCCESS) {
		efi_err("Failed to install dragonstub_payload_efi\n");
		return status;
	}

	return EFI_SUCCESS;
}