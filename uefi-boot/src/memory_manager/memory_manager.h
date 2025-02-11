#pragma once
#include <Library/UefiLib.h>

EFI_STATUS mm_allocate_pool(VOID** buffer_out, UINT64 size, EFI_MEMORY_TYPE memory_type);
EFI_STATUS mm_free_pool(VOID* buffer);
