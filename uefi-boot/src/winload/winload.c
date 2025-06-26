#include "winload.h"

#include <Library/UefiBootServicesTableLib.h>

#include "../hooks/hooks.h"
#include "../image/image.h"
#include "../bootmgfw/bootmgfw.h"
#include "../structures/ntdef.h"

#include "../hyperv_attachment/hyperv_attachment.h"
#include "../hvloader/hvloader.h"

UINT64 pml4_physical_allocation = 0;
UINT64 pdpt_physical_allocation = 0;

hook_data_t winload_load_pe_image_hook_data = { 0 };

CHAR8* bl_allocate_slab_pages = NULL;
CHAR8* bl_mm_translate_virtual_address_ex = NULL;

typedef UINT64(*bl_allocate_slab_pages_t)(UINT64* allocation_base_out, UINT64 pages_to_map, UINT64 a3, UINT64 a4);
typedef UINT64(*bl_mm_translate_virtual_address_ex_t)(UINT64 virtual_address, UINT64* physical_address_out, UINT64 a3, UINT64 a4);
typedef EFI_ALLOCATE_PAGES efi_allocate_pages_t;

efi_allocate_pages_t original_bs_allocate_pages = NULL;

UINT64 winload_translate_virtual_address(UINT64* physical_address_out, UINT64 virtual_address)
{
    return ((bl_mm_translate_virtual_address_ex_t)(bl_mm_translate_virtual_address_ex))(virtual_address, physical_address_out, 0, 0);
}

UINT64 winload_allocate_slab_pages_virtual(UINT64* virtual_allocation_base_out, UINT64 pages_to_map)
{
    if (virtual_allocation_base_out == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    return ((bl_allocate_slab_pages_t)bl_allocate_slab_pages)(virtual_allocation_base_out, pages_to_map, 0, 0);
}

EFI_STATUS winload_bs_allocate_pages_detour(EFI_ALLOCATE_TYPE type, EFI_MEMORY_TYPE memory_type, UINT64 pages, EFI_PHYSICAL_ADDRESS* memory)
{
    EFI_MEMORY_TYPE new_memory_type = memory_type;

    if (memory_type == EfiLoaderCode)
    {
        new_memory_type = EfiRuntimeServicesCode;
    }
    else if (memory_type == EfiLoaderData)
    {
        new_memory_type = EfiRuntimeServicesData;
    }

    return original_bs_allocate_pages(type, new_memory_type, pages, memory);
}

void winload_place_bs_allocate_pages_hook()
{
    gBS->AllocatePages = winload_bs_allocate_pages_detour;
}

void winload_remove_bs_allocate_pages_hook()
{
    gBS->AllocatePages = original_bs_allocate_pages;
}

UINT64 winload_allocate_slab_pages_physical(UINT64* physical_allocation_base_out, UINT64* virtual_allocation_base_out, UINT64 pages_to_map)
{
    if (physical_allocation_base_out == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    original_bs_allocate_pages = gBS->AllocatePages;

    winload_place_bs_allocate_pages_hook();

    UINT64 status = winload_allocate_slab_pages_virtual(virtual_allocation_base_out, pages_to_map);

    winload_remove_bs_allocate_pages_hook();

    if (status == EFI_SUCCESS)
    {
        winload_translate_virtual_address(physical_allocation_base_out, *virtual_allocation_base_out);
    }

    return status;
}

UINT64 winload_load_pe_image_detour(bl_file_info_t* file_info, INT32 a2, UINT64* image_base, UINT32* image_size, UINT64* a5, UINT32* a6, UINT32* a7, UINT64 a8, UINT64 a9, unknown_param_t a10, unknown_param_t a11, unknown_param_t a12, unknown_param_t a13, unknown_param_t a14, unknown_param_t a15)
{
    hook_disable(&winload_load_pe_image_hook_data);

    boot_load_pe_image_t original_subroutine = (boot_load_pe_image_t)winload_load_pe_image_hook_data.hooked_subroutine_address;

    UINT64 return_value = original_subroutine(file_info, a2, image_base, image_size, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);

    if (StrStr(file_info->file_name, L"hvloader") != NULL)
    {
        hvloader_place_hooks(*image_base, *image_size);

        return return_value;
    }

    hook_enable(&winload_load_pe_image_hook_data);

    return return_value;
}

EFI_STATUS winload_place_load_pe_image_hook(UINT64 image_base, UINT64 image_size)
{
    CHAR8* code_ref_to_load_pe_image = NULL;

    // ImgpLoadPEImage
    EFI_STATUS status = scan_image(&code_ref_to_load_pe_image, (CHAR8*)image_base, image_size, d_boot_load_pe_image_pattern, d_boot_load_pe_image_mask);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    CHAR8* load_pe_image_subroutine = (code_ref_to_load_pe_image + 10) + *(UINT32*)(code_ref_to_load_pe_image + 6);

    status = hook_create(&winload_load_pe_image_hook_data, load_pe_image_subroutine, (void*)winload_load_pe_image_detour);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    return hook_enable(&winload_load_pe_image_hook_data);
}

EFI_STATUS winload_place_hooks(UINT64 image_base, UINT64 image_size)
{
    UINT8* code_ref_to_allocate_slab_pages = NULL;

    EFI_STATUS status = scan_image(&code_ref_to_allocate_slab_pages, (CHAR8*)image_base, image_size, "\xC1\xE9\x00\x8D\x14\x01\x48\x8B\x0D\x00\x00\x00\x00\x48\x81\xC1\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\xF8\x85\xC0\x0F\x88", "xx?xxxxxx????xxx????x????xxxxxx");

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    bl_allocate_slab_pages = (code_ref_to_allocate_slab_pages + 25) + *(INT32*)(code_ref_to_allocate_slab_pages + 21);

    UINT8* code_ref_to_translate_virt_address_ex = NULL;

    status = scan_image(&code_ref_to_translate_virt_address_ex, (CHAR8*)image_base, image_size, "\xE8\x00\x00\x00\x00\x48\x8D\x83\x00\x00\x00\x00\x48\x89\x1D", "x????xxx????xxx");

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    bl_mm_translate_virtual_address_ex = (code_ref_to_translate_virt_address_ex + 5) + *(INT32*)(code_ref_to_translate_virt_address_ex + 1);

    return winload_place_load_pe_image_hook(image_base, image_size);
}