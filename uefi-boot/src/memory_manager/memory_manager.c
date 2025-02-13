#include "memory_manager.h"
#include <Library/UefiBootServicesTableLib.h>

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
	for (UINT64 i = 0; i < size; i++)
	{
		destination[i] = source[i];
	}
}
