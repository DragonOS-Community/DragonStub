#pragma once

#include <elf.h>
#include "types.h"

struct payload_info;

bool elf_check(const void *payload_start, u64 payload_size);

/// @brief 获取ELF文件头
/// @param payload_start 文件起始地址
/// @param payload_size 文件大小
/// @param ehdr 返回的ELF文件头
/// @return
efi_status_t elf_get_header(const void *payload_start, u64 payload_size,
			    Elf64_Ehdr **ehdr);

efi_status_t load_elf(struct payload_info *payload_info);