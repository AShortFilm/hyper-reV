#pragma once
#include <Library/UefiLib.h>
#include <ia32-doc/ia32_compact.h>
#include "../structures/virtual_address.h"

extern UINT8* hyperv_attachment_file_buffer;

extern UINT64 hyperv_attachment_heap_physical_allocation;
extern UINT32 hyperv_attachment_heap_4kb_pages_needed;

EFI_STATUS hyperv_attachment_allocate_and_copy();
EFI_STATUS hyperv_attachment_get_relocated_entry_point(UINT8** hyperv_attachment_entry_point);
void hyperv_attachment_invoke_entry_point(UINT8** hyperv_attachment_vmexit_handler_detour_out, UINT8* hyperv_attachment_entry_point, CHAR8* original_vmexit_handler, UINT64 heap_physical_base, UINT64 heap_size);
EFI_STATUS hyperv_attachment_load_and_delete_from_disk(UINT8** file_buffer_out);

