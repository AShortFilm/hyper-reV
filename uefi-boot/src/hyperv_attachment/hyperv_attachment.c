#include "hyperv_attachment.h"
#include "../memory_manager/memory_manager.h"
#include "../structures/relocation_entry.h"
#include "../image/image.h"
#include "../winload/winload.h"
#include "../disk/disk.h"

UINT8* hyperv_attachment_file_buffer = NULL;
UINT8* hyperv_attachment_physical_base = NULL;

UINT64 hyperv_attachment_heap_physical_allocation = 0;
UINT32 hyperv_attachment_heap_4kb_pages_needed = 32;

#define d_hyperv_attachment_path L"\\efi\\microsoft\\boot\\hyperv-attachment.dll"

UINT64 get_pages_needed_for_hyperv_attachment(EFI_IMAGE_NT_HEADERS64* nt_headers)
{
    UINT8 is_page_aligned = (nt_headers->OptionalHeader.SizeOfImage % 0x1000) == 0;

    UINT64 pages_needed = (nt_headers->OptionalHeader.SizeOfImage & ~0xFFF) / 0x1000;

    if (is_page_aligned == 0)
    {
        pages_needed += 1;
    }

    return pages_needed;
}

EFI_STATUS hyperv_attachment_allocate_and_copy()
{
    EFI_IMAGE_NT_HEADERS64* nt_headers = image_get_nt_headers(hyperv_attachment_file_buffer);
    
    UINT64 pages_needed = get_pages_needed_for_hyperv_attachment(nt_headers);

    UINT64 hyperv_attachment_virtual_base = 0;

    EFI_STATUS status = winload_allocate_slab_pages_physical((UINT64*)&hyperv_attachment_physical_base, &hyperv_attachment_virtual_base, pages_needed);

    if (status == EFI_SUCCESS)
    {
        mm_copy_memory((UINT8*)hyperv_attachment_virtual_base, hyperv_attachment_file_buffer, nt_headers->OptionalHeader.SizeOfHeaders);

        EFI_IMAGE_SECTION_HEADER* current_section = (EFI_IMAGE_SECTION_HEADER*)((UINT8*)&nt_headers->OptionalHeader + nt_headers->FileHeader.SizeOfOptionalHeader);

        for (UINT32 i = 0; i < nt_headers->FileHeader.NumberOfSections; i++, current_section++)
        {
            mm_copy_memory((UINT8*)hyperv_attachment_virtual_base + current_section->VirtualAddress, hyperv_attachment_file_buffer + current_section->PointerToRawData, current_section->SizeOfRawData);
        }
    }

    // just let exitbootservices manage the deallocation for us
    //mm_free_pool(hyperv_attachment_file_buffer);

    return status;
}

EFI_STATUS hyperv_attachment_remap_image(UINT8** hyperv_attachment_virtual_base_out)
{
    if (hyperv_attachment_virtual_base_out == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    const UINT64 physical_memory_access_base = 255ull << 39;

    *hyperv_attachment_virtual_base_out = physical_memory_access_base + hyperv_attachment_physical_base;

    return EFI_SUCCESS;
}

EFI_STATUS hyperv_attachment_apply_relocation(UINT8* hyperv_attachment_virtual_base)
{
    EFI_IMAGE_NT_HEADERS64* nt_headers = image_get_nt_headers(hyperv_attachment_virtual_base);

    EFI_IMAGE_DATA_DIRECTORY* base_relocation_directory = &nt_headers->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (base_relocation_directory->VirtualAddress != 0)
    {
        UINT64 relocation_to_apply = (UINT64)hyperv_attachment_virtual_base - nt_headers->OptionalHeader.ImageBase;

        EFI_IMAGE_BASE_RELOCATION* base_relocation = (EFI_IMAGE_BASE_RELOCATION*)(hyperv_attachment_virtual_base + base_relocation_directory->VirtualAddress);

        while (base_relocation->VirtualAddress)
        {
            relocation_entry_t* current_relocation_entry = (relocation_entry_t*)(base_relocation + 1);
            UINT32 entry_count = (base_relocation->SizeOfBlock - sizeof(EFI_IMAGE_BASE_RELOCATION)) / sizeof(UINT16);

            for (UINT32 i = 0; i < entry_count; i++, current_relocation_entry++)
            {
                if (current_relocation_entry->type == EFI_IMAGE_REL_BASED_DIR64)
                {
                    *(UINT64*)(hyperv_attachment_virtual_base + base_relocation->VirtualAddress + current_relocation_entry->offset) += relocation_to_apply;
                }
            }

            base_relocation = (EFI_IMAGE_BASE_RELOCATION*)((UINT8*)base_relocation + base_relocation->SizeOfBlock);
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS hyperv_attachment_get_entry_point(UINT8** entry_point_out, UINT8* hyperv_attachment_virtual_base)
{
    if (entry_point_out == NULL || hyperv_attachment_virtual_base == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_IMAGE_NT_HEADERS64* nt_headers = image_get_nt_headers(hyperv_attachment_virtual_base);

    *entry_point_out = hyperv_attachment_virtual_base + nt_headers->OptionalHeader.AddressOfEntryPoint;

    return EFI_SUCCESS;
}

EFI_STATUS hyperv_attachment_get_relocated_entry_point(UINT8** hyperv_attachment_entry_point)
{
    UINT8* hyperv_attachment_virtual_base = NULL;

    EFI_STATUS status = hyperv_attachment_remap_image(&hyperv_attachment_virtual_base);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    status = hyperv_attachment_apply_relocation(hyperv_attachment_virtual_base);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    return hyperv_attachment_get_entry_point(hyperv_attachment_entry_point, hyperv_attachment_virtual_base);
}

typedef void(*hyperv_attachment_entry_point_t)(UINT8** vmexit_handler_detour_out, UINT8* original_vmexit_handler_routine, UINT64 heap_physical_base, UINT64 heap_size);

void hyperv_attachment_invoke_entry_point(UINT8** hyperv_attachment_vmexit_handler_detour_out, UINT8* hyperv_attachment_entry_point, CHAR8* original_vmexit_handler, UINT64 heap_physical_base, UINT64 heap_size)
{
    ((hyperv_attachment_entry_point_t)(hyperv_attachment_entry_point))(hyperv_attachment_vmexit_handler_detour_out, original_vmexit_handler, heap_physical_base, heap_size);
}

EFI_STATUS hyperv_attachment_load_and_delete_from_disk(UINT8** file_buffer_out)
{
    if (file_buffer_out == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_FILE_PROTOCOL* file_handle = NULL;

    EFI_HANDLE device_handle = NULL;

    EFI_STATUS status = disk_open_file(&file_handle, &device_handle, d_hyperv_attachment_path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, EFI_FILE_SYSTEM);

    if (status == EFI_SUCCESS)
    {
        EFI_FILE_INFO* file_info = NULL;
        UINT64 file_info_size = 0;

        status = disk_get_generic_file_info(&file_info, &file_info_size, file_handle);

        if (status == EFI_SUCCESS)
        {
            UINT64 file_size = file_info->FileSize;

            mm_free_pool(file_info);

            if (file_size < sizeof(EFI_IMAGE_DOS_HEADER))
            {
                status = EFI_NOT_FOUND;
            }
            else
            {
                status = disk_load_file(file_handle, file_buffer_out, file_size);

                if (status == EFI_SUCCESS)
                {
                    EFI_IMAGE_DOS_HEADER* dos_header = (EFI_IMAGE_DOS_HEADER*)(*file_buffer_out);

                    if (dos_header->e_magic != 0x5a4d)
                    {
                        mm_free_pool(*file_buffer_out);

                        status = EFI_NOT_FOUND;
                    }
                }
            }
        }

        disk_delete_file(file_handle);
    }

	return status;
}
