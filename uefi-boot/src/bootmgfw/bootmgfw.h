#pragma once
#include <Library/UefiLib.h>

EFI_STATUS bootmgfw_restore_original_file(EFI_HANDLE* device_handle_out);
EFI_STATUS bootmgfw_run_original_image(EFI_HANDLE parent_image_handle, EFI_HANDLE device_handle);
