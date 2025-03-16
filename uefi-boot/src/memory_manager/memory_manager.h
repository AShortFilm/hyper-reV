#pragma once
#include <Library/UefiLib.h>

EFI_STATUS mm_allocate_pages(VOID** buffer_out, UINT64 page_count, EFI_MEMORY_TYPE memory_type);
EFI_STATUS mm_allocate_pool(VOID** buffer_out, UINT64 size, EFI_MEMORY_TYPE memory_type);
EFI_STATUS mm_free_pool(VOID* buffer);
void mm_copy_memory(UINT8* destination, UINT8* source, UINT64 size);
