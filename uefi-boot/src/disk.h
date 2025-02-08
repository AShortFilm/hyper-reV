#pragma once
#include <Library/ShellLib.h>

EFI_STATUS disk_get_all_file_system_handles(EFI_HANDLE** handle_list_out, UINT64* handle_count_out);
EFI_STATUS disk_open_file_system(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL** file_system_out, EFI_HANDLE handle);
EFI_STATUS disk_open_volume(EFI_FILE_PROTOCOL** volume_handle_out, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* file_system);
EFI_STATUS disk_open_file_on_volume(EFI_FILE_PROTOCOL** file_handle_out, EFI_FILE_PROTOCOL* volume, CHAR16* file_path, UINT64 open_mode, UINT64 attributes);
EFI_STATUS disk_open_file(EFI_FILE_PROTOCOL** file_handle_out, EFI_HANDLE* device_handle_out_opt, CHAR16* file_path, UINT64 open_mode, UINT64 attributes);
EFI_STATUS disk_get_device_path(EFI_DEVICE_PATH** device_path_out, EFI_HANDLE device_handle, CHAR16* file_path);
