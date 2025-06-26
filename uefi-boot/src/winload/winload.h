#pragma once
#include <Library/UefiLib.h>
#include <ia32-doc/ia32_compact.h>

extern UINT64 pml4_physical_allocation;
extern UINT64 pdpt_physical_allocation;

UINT64 winload_translate_virtual_address(UINT64* physical_address_out, UINT64 virtual_address);
UINT64 winload_allocate_slab_pages_physical(UINT64* physical_allocation_base_out, UINT64* virtual_allocation_base_out, UINT64 pages_to_map);
EFI_STATUS winload_place_hooks(UINT64 image_base, UINT64 image_size);
