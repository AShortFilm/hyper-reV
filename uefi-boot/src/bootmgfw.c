#include "bootmgfw.h"
#include "memory_manager.h"
#include "image.h"
#include "disk.h"

#define d_bootmgfw_path L"\\efi\\microsoft\\boot\\bootmgfw.efi"
#define d_path_original_bootmgfw L"\\efi\\microsoft\\boot\\bootmgfw.original.efi"

EFI_STATUS read_original_bootmgfw_to_buffer(EFI_FILE_PROTOCOL* bootmgfw_original_file, void** buffer, UINT64 buffer_size)
{
    EFI_STATUS status = mm_allocate_pool(buffer, buffer_size, EfiBootServicesData);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    status = disk_read_file(bootmgfw_original_file, *buffer, buffer_size);

    if (status != EFI_SUCCESS)
    {
        mm_free_pool(*buffer);

        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS write_original_bootmgfw_back(EFI_FILE_INFO* original_bootmgfw_file_info, EFI_FILE_PROTOCOL* bootmgfw_file, void* buffer, UINT64 buffer_size)
{
    EFI_STATUS status = disk_write_file(bootmgfw_file, buffer, buffer_size);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    EFI_FILE_INFO* bootmgfw_file_info = NULL;
    UINT64 file_info_size = 0;

    status = disk_get_generic_file_info(&bootmgfw_file_info, &file_info_size, bootmgfw_file);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    bootmgfw_file_info->CreateTime = original_bootmgfw_file_info->CreateTime;
    bootmgfw_file_info->ModificationTime = original_bootmgfw_file_info->ModificationTime;

    status = disk_set_generic_file_info(bootmgfw_file, bootmgfw_file_info, file_info_size);

    mm_free_pool(bootmgfw_file_info);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS bootmgfw_restore_original_file(EFI_HANDLE* device_handle_out)
{
    EFI_FILE_PROTOCOL* bootmgfw_original_file = NULL;

    EFI_STATUS status = disk_open_file(&bootmgfw_original_file, device_handle_out, d_path_original_bootmgfw, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, EFI_FILE_SYSTEM);

    if (status == EFI_SUCCESS)
    {
        EFI_FILE_INFO* original_bootmgfw_file_info = NULL;
        UINT64 file_info_size = 0;

        status = disk_get_generic_file_info(&original_bootmgfw_file_info, &file_info_size, bootmgfw_original_file);

        if (status == EFI_SUCCESS)
        {
            UINT64 original_bootmgfw_buffer_size = original_bootmgfw_file_info->FileSize;
            void* original_bootmgfw_buffer = NULL;

            status = read_original_bootmgfw_to_buffer(bootmgfw_original_file, &original_bootmgfw_buffer, original_bootmgfw_buffer_size);

            if (status == EFI_SUCCESS)
            {
                EFI_FILE_PROTOCOL* bootmgfw_file = NULL;

                status = disk_open_file(&bootmgfw_file, device_handle_out, d_bootmgfw_path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, EFI_FILE_SYSTEM);

                if (status == EFI_SUCCESS)
                {
                    status = write_original_bootmgfw_back(original_bootmgfw_file_info, bootmgfw_file, original_bootmgfw_buffer, original_bootmgfw_buffer_size);

                    disk_close_file(bootmgfw_file);
                }

                mm_free_pool(original_bootmgfw_buffer);
            }

            mm_free_pool(original_bootmgfw_file_info);
        }

        disk_delete_file(bootmgfw_original_file);
    }

    return status;
}

EFI_STATUS bootmgfw_run_original_image(EFI_HANDLE parent_image_handle, EFI_HANDLE device_handle)
{
    EFI_DEVICE_PATH* device_path = NULL;

    EFI_STATUS status = disk_get_device_path(&device_path, device_handle, d_bootmgfw_path);

    if (status == EFI_SUCCESS)
    {
        EFI_HANDLE loaded_image = NULL;

        status = load_image(&loaded_image, TRUE, parent_image_handle, device_path);

        if (status == EFI_SUCCESS)
        {
            status = start_image(loaded_image);
        }
        else
        {
            unload_image(loaded_image);
        }
    }

    return status;
}