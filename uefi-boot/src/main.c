#include <Library/UefiLib.h>

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
    Print(L"Hello World!\n");

    return EFI_SUCCESS;
}
