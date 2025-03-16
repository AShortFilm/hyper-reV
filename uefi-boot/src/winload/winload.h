#pragma once
#include <Library/UefiLib.h>
#include <ia32-doc/ia32_compact.h>

extern pml4e_64* pml4_allocation;
extern pdpte_64* pdpt_identity_map_allocation;

EFI_STATUS winload_place_hooks(UINT64 image_base, UINT64 image_size);
