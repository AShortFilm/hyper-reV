#pragma once
#include <Library/UefiLib.h>

EFI_STATUS winload_place_hooks(UINT64 image_base, UINT64 image_size);
