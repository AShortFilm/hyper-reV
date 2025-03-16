#include "memory_manager.h"
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS mm_allocate_pages(VOID** buffer_out, UINT64 page_count, EFI_MEMORY_TYPE memory_type)
{
	return gBS->AllocatePages(AllocateAnyPages, memory_type, page_count, (EFI_PHYSICAL_ADDRESS*)buffer_out);
}

EFI_STATUS mm_allocate_pool(VOID** buffer_out, UINT64 size, EFI_MEMORY_TYPE memory_type)
{
	return gBS->AllocatePool(memory_type, size, buffer_out);
}

EFI_STATUS mm_free_pool(VOID* buffer)
{
	return gBS->FreePool(buffer);
}

void mm_copy_memory(UINT8* destination, UINT8* source, UINT64 size)
{
	UINT64 original_cr0 = AsmReadCr0();
	AsmWriteCr0(original_cr0 & ~0x10000);

	for (UINT64 i = 0; i < size; i++)
	{
		destination[i] = source[i];
	}

	AsmWriteCr0(original_cr0);
}
