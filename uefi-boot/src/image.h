#pragma once
#include <Library/UefiLib.h>

EFI_STATUS load_image(EFI_HANDLE* loaded_image_handle_out, BOOLEAN boot_policy, EFI_HANDLE parent_image_handle, EFI_DEVICE_PATH* device_path);
EFI_STATUS unload_image(EFI_HANDLE image_handle);
EFI_STATUS start_image(EFI_HANDLE image_handle);
