/*++

Copyright (c) 1998  Intel Corporation

Module Name:


Abstract:




Revision History

--*/

#include "lib.h"
#include <dragonstub/limits.h>

VOID EFIDebugVariable(VOID);

VOID InitializeLib(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
/*++

Routine Description:

    Initializes EFI library for use
    
Arguments:

    Firmware's EFI system table
    
Returns:

    None

--*/
{
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_STATUS Status;
	CHAR8 *LangCode;

	if (LibInitialized)
		return;

	LibInitialized = TRUE;
	LibFwInstance = FALSE;
	LibImageHandle = ImageHandle;

	//
	// Set up global pointer to the system table, boot services table,
	// and runtime services table
	//

	ST = SystemTable;
	BS = SystemTable->BootServices;
	RT = SystemTable->RuntimeServices;
	// ASSERT (CheckCrc(0, &ST->Hdr));
	// ASSERT (CheckCrc(0, &BS->Hdr));
	// ASSERT (CheckCrc(0, &RT->Hdr));

	//
	// Initialize pool allocation type
	//

	if (ImageHandle) {
		Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle,
					   &LoadedImageProtocol,
					   (VOID *)&LoadedImage);

		if (!EFI_ERROR(Status)) {
			PoolAllocationType = LoadedImage->ImageDataType;
		}
		EFIDebugVariable();
	}

	//
	// Initialize Guid table
	//

	InitializeGuid();

	InitializeLibPlatform(ImageHandle, SystemTable);

	if (ImageHandle && UnicodeInterface == &LibStubUnicodeInterface) {
		LangCode = LibGetVariable(VarLanguage, &EfiGlobalVariable);
		InitializeUnicodeSupport(LangCode);
		if (LangCode) {
			FreePool(LangCode);
		}
	}
}

VOID InitializeUnicodeSupport(CHAR8 *LangCode)
{
	EFI_UNICODE_COLLATION_INTERFACE *Ui;
	EFI_STATUS Status;
	CHAR8 *Languages;
	UINTN Index, Position, Length;
	UINTN NoHandles;
	EFI_HANDLE *Handles;

	//
	// If we don't know it, lookup the current language code
	//

	LibLocateHandle(ByProtocol, &UnicodeCollationProtocol, NULL, &NoHandles,
			&Handles);
	if (!LangCode || !NoHandles) {
		goto Done;
	}

	//
	// Check all driver's for a matching language code
	//

	for (Index = 0; Index < NoHandles; Index++) {
		Status = uefi_call_wrapper(BS->HandleProtocol, 3,
					   Handles[Index],
					   &UnicodeCollationProtocol,
					   (VOID *)&Ui);
		if (EFI_ERROR(Status)) {
			continue;
		}

		//
		// Check for a matching language code
		//

		Languages = Ui->SupportedLanguages;
		Length = strlena(Languages);
		for (Position = 0; Position < Length;
		     Position += ISO_639_2_ENTRY_SIZE) {
			//
			// If this code matches, use this driver
			//

			if (CompareMem(Languages + Position, LangCode,
				       ISO_639_2_ENTRY_SIZE) == 0) {
				UnicodeInterface = Ui;
				goto Done;
			}
		}
	}

Done:
	//
	// Cleanup
	//

	if (Handles) {
		FreePool(Handles);
	}
}

VOID EFIDebugVariable(VOID)
{
	EFI_STATUS Status;
	UINT32 Attributes;
	UINTN DataSize;
	UINTN NewEFIDebug;

	DataSize = sizeof(EFIDebug);
	Status = uefi_call_wrapper(RT->GetVariable, 5, L"EFIDebug",
				   &EfiGlobalVariable, &Attributes, &DataSize,
				   &NewEFIDebug);
	if (!EFI_ERROR(Status)) {
		EFIDebug = NewEFIDebug;
	}
}

/*
 * Calls to memset/memcpy may be emitted implicitly by GCC or MSVC
 * even when -ffreestanding or /NODEFAULTLIB are in effect.
 */

#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ UINTN
#endif

void *memset(void *s, int c, __SIZE_TYPE__ n)
{
	unsigned char *p = s;

	while (n--)
		*p++ = c;

	return s;
}

void *memcpy(void *dest, const void *src, __SIZE_TYPE__ n)
{
	const unsigned char *q = src;
	unsigned char *p = dest;

	while (n--)
		*p++ = *q++;

	return dest;
}

/**
 * @brief 将数据从src搬运到dst，并能正确处理地址重叠的问题
 *
 * @param dst 目标地址指针
 * @param src 源地址指针
 * @param size 大小
 * @return void* 指向目标地址的指针
 */
void *memmove(void *dst, const void *src, uint64_t size)
{
	const char *_src = src;
	char *_dst = dst;

	if (!size)
		return dst;

	// 当源地址大于目标地址时，使用memcpy来完成
	if (dst <= src)
		return memcpy(dst, src, size);

	// 当源地址小于目标地址时，为防止重叠覆盖，因此从后往前拷贝
	_src += size;
	_dst += size;

	// 逐字节拷贝
	while (size--)
		*--_dst = *--_src;

	return dst;
}

#define SS (sizeof(size_t))
#define __ALIGN (sizeof(size_t)-1)
#define ONES ((size_t)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

void *memchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	c = (unsigned char)c;
#ifdef __GNUC__
	for (; ((uintptr_t)s & __ALIGN) && n && *s != c; s++, n--);
	if (n && *s != c) {
		typedef size_t __attribute__((__may_alias__)) word;
		const word *w;
		size_t k = ONES * c;
		for (w = (const void *)s; n>=SS && !HASZERO(*w^k); w++, n-=SS);
		s = (const void *)w;
	}
#endif
	for (; n && *s != c; s++, n--);
	return n ? (void *)s : 0;
}

int memcmp(const void *vl, const void *vr, size_t n)
{
	const unsigned char *l = vl, *r = vr;
	for (; n && *l == *r; n--, l++, r++)
		;
	return n ? *l - *r : 0;
}

void *memrchr(const void *m, int c, size_t n)
{
	const unsigned char *s = m;
	c = (unsigned char)c;
	while (n--) if (s[n]==c) return (void *)(s+n);
	return 0;
}
