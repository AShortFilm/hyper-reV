#include "hvloader.h"
#include "../hooks/hooks.h"
#include "../image/image.h"
#include "../structures/virtual_address.h"
#include "../memory_manager/memory_manager.h"
#include "../hyperv_attachment/hyperv_attachment.h"
#include "../winload/winload.h"

#include <IndustryStandard/PeImage.h>

hook_data_t hvloader_launch_hv_hook_data = { 0 };
hook_data_t hv_vmexit_hook_data = { 0 };

typedef void(*hvloader_launch_hv_t)(cr3 a1, virtual_address_t a2, UINT64 a3, UINT64 a4);

UINT64 get_hyperv_base(virtual_address_t entry_point)
{
    entry_point.offset = 0;

    for (UINT64 hyperv_4kb_boundary = entry_point.address; hyperv_4kb_boundary != 0; hyperv_4kb_boundary -= 0x1000)
    {
        UINT16 header_magic = *(UINT16*)(hyperv_4kb_boundary);

        if (header_magic == 0x5a4d)
        {
            return hyperv_4kb_boundary;
        }
    }

    return 0;
}

void set_up_identity_map(pml4e_64* pml4e)
{
    pdpte_64* pdpt = (pdpte_64*)pdpt_virtual_identity_map_allocation;

    pml4e->flags = 0;
    pml4e->page_frame_number = (UINT64)pdpt_physical_identity_map_allocation >> 12;
    pml4e->present = 1;
    pml4e->write = 1;

    for (UINT64 i = 0; i < 512; i++)
    {
        pdpte_64* pdpte = &pdpt[i];

        pdpte->flags = 0;
        pdpte->page_frame_number = i;
        pdpte->present = 1;
        pdpte->write = 1;
        pdpte->large_page = 1;
    }
}

void load_identity_map_into_hyperv_cr3(cr3 identity_map_cr3, cr3 hyperv_cr3, pml4e_64 identity_map_pml4e)
{
    AsmWriteCr3(identity_map_cr3.flags);

    pml4e_64* hyperv_pml4 = (pml4e_64*)(hyperv_cr3.address_of_page_directory << 12);

    hyperv_pml4[0] = identity_map_pml4e;
}

void set_up_hyperv_hooks(cr3 hyperv_cr3, virtual_address_t entry_point)
{
    AsmWriteCr3(hyperv_cr3.flags);

    UINT64 hyperv_base = get_hyperv_base(entry_point);

    if (hyperv_base != 0)
    {
        UINT8* hyperv_attachment_entry_point = NULL;

        EFI_STATUS status = hyperv_attachment_get_relocated_entry_point(&hyperv_attachment_entry_point);

        if (status == EFI_SUCCESS)
        {
            EFI_IMAGE_DOS_HEADER* dos_header = (EFI_IMAGE_DOS_HEADER*)hyperv_base;

            EFI_IMAGE_NT_HEADERS64* nt_headers = (EFI_IMAGE_NT_HEADERS64*)(hyperv_base + dos_header->e_lfanew);

            UINT64 size_of_image = (UINT64)nt_headers->OptionalHeader.SizeOfImage;

            CHAR8* code_ref_to_vmexit_handler = NULL;

            status = scan_image(&code_ref_to_vmexit_handler, (CHAR8*)hyperv_base, size_of_image, "\xE8\x00\x00\x00\x00\xE9\x00\x00\x00\x00\x74", "x????x????x");

            if (status == EFI_SUCCESS)
            {
                INT32 original_vmexit_handler_rva = *(INT32*)(code_ref_to_vmexit_handler + 1);
                CHAR8* original_vmexit_handler = (code_ref_to_vmexit_handler + 5) + original_vmexit_handler_rva;

                UINT8* hyperv_attachment_vmexit_handler_detour = NULL;

                hyperv_attachment_invoke_entry_point(&hyperv_attachment_vmexit_handler_detour, hyperv_attachment_entry_point, original_vmexit_handler);

                CHAR8* code_cave = NULL;

                status = scan_image(&code_cave, (CHAR8*)hyperv_base, size_of_image, "\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC", "xxxxxxxxxxxxxxxx");

                if (status == EFI_SUCCESS)
                {
                    status = hook_create(&hv_vmexit_hook_data, code_cave, hyperv_attachment_vmexit_handler_detour);

                    if (status == EFI_SUCCESS)
                    {
                        hook_enable(&hv_vmexit_hook_data);

                        UINT32 new_call_rva = (UINT32)(code_cave - (code_ref_to_vmexit_handler + 5));

                        mm_copy_memory(code_ref_to_vmexit_handler + 1, (UINT8*)&new_call_rva, sizeof(new_call_rva));
                    }
                }
            }
        }
    }
}

void hvloader_launch_hv_detour(cr3 hyperv_cr3, virtual_address_t hyperv_entry_point, UINT64 jmp_gadget, UINT64 kernel_cr3)
{
    hook_disable(&hvloader_launch_hv_hook_data);

    pml4e_64* virtual_pml4 = (pml4e_64*)pml4_virtual_allocation;

    set_up_identity_map(&virtual_pml4[0]);

    UINT64 original_cr3 = AsmReadCr3();

    cr3 identity_map_cr3 = { .address_of_page_directory = pml4_physical_allocation >> 12 };

    load_identity_map_into_hyperv_cr3(identity_map_cr3, hyperv_cr3, virtual_pml4[0]);

    set_up_hyperv_hooks(hyperv_cr3, hyperv_entry_point);

    AsmWriteCr3(original_cr3);

    hvloader_launch_hv_t original_subroutine = (hvloader_launch_hv_t)hvloader_launch_hv_hook_data.hooked_subroutine_address;

    original_subroutine(hyperv_cr3, hyperv_entry_point, jmp_gadget, kernel_cr3);
}

EFI_STATUS hvloader_place_hooks(UINT64 image_base, UINT64 image_size)
{
    CHAR8* hvloader_launch_hv = NULL;

    EFI_STATUS status = scan_image(&hvloader_launch_hv, (CHAR8*)image_base, image_size, "\x48\x53\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x00\x48\x89\x25", "xxxxxxxxxxxxxxxx?xxx");

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    status = hook_create(&hvloader_launch_hv_hook_data, hvloader_launch_hv, (void*)hvloader_launch_hv_detour);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    return hook_enable(&hvloader_launch_hv_hook_data);
}
