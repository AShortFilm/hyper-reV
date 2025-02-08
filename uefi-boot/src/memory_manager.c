#include "memory_manager.h"
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS mm_free_pool(VOID* buffer)
{
	return gBS->FreePool(buffer);
}
