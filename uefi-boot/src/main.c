#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "disk.h"

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

    EFI_STATUS status = disk_open_file(&virtual_file, L"sample_file.txt", EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_SYSTEM | EFI_FILE_HIDDEN);

    return status;
}
