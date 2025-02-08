#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "disk.h"
#include "image.h"

#define d_bootmgfw_path L"\\efi\\microsoft\\boot\\bootmgfw.efi"

const UINT8 _gDriverUnloadImageCount = 1;
const UINT32 _gUefiDriverRevision = 0x200;
CHAR8* gEfiCallerBaseName = "hyper-reV";

EFI_STATUS
EFIAPI
UefiUnload(
    IN EFI_HANDLE image_handle
)
{
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE image_handle,
    IN EFI_SYSTEM_TABLE* system_table
)
{
    EFI_FILE_PROTOCOL* virtual_file = NULL;
    EFI_HANDLE device_handle = NULL;

    CHAR16* file_name = d_bootmgfw_path;

    EFI_STATUS status = disk_open_file(&virtual_file, &device_handle, file_name, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_SYSTEM | EFI_FILE_HIDDEN);

    if (status == EFI_SUCCESS)
    {
        EFI_DEVICE_PATH* device_path = NULL;

        status = disk_get_device_path(&device_path, device_handle, file_name);

        if (status == EFI_SUCCESS)
        {
            EFI_HANDLE loaded_image = NULL;

            status = load_image(&loaded_image, TRUE, image_handle, device_path);

            if (status == EFI_SUCCESS)
            {
                status = start_image(loaded_image);
            }
        }
    }

    return status;
}
