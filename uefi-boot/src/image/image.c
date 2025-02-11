#include "image.h"
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS load_image(EFI_HANDLE* loaded_image_handle_out, BOOLEAN boot_policy, EFI_HANDLE parent_image_handle, EFI_DEVICE_PATH* device_path)
{
    return gBS->LoadImage(boot_policy, parent_image_handle, device_path, NULL, 0, loaded_image_handle_out);
}

EFI_STATUS unload_image(EFI_HANDLE image_handle)
{
    return gBS->UnloadImage(image_handle);
}

EFI_STATUS start_image(EFI_HANDLE image_handle)
{
    return gBS->StartImage(image_handle, NULL, NULL);
}
