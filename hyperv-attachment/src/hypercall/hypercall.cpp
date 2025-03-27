#include "hypercall.h"
#include <hypercall/hypercall_def.h>
#include "../memory_manager/memory_manager.h"
#include "../slat/slat.h"
#include "../arch/arch.h"
#include "../crt/crt.h"

std::uint64_t read_guest_physical_memory(trap_frame_t* trap_frame)
{
    cr3 guest_cr3 = arch::get_guest_cr3();
    cr3 slat_cr3 = slat::get_cr3();

    std::uint64_t guest_destination_virtual_address = trap_frame->r8;
    std::uint64_t guest_source_physical_address = trap_frame->rdx;

    std::uint64_t size_left_to_read = trap_frame->r9;

    std::uint64_t bytes_read = 0;

    while (size_left_to_read != 0)
    {
        std::uint64_t size_left_of_destination_slat_page = 0;
        std::uint64_t size_left_of_source_slat_page = 0;

        std::uint64_t guest_destination_physical_address = memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = guest_destination_virtual_address + bytes_read });

        std::uint64_t host_destination = memory_manager::map_guest_physical(slat_cr3, guest_destination_physical_address, &size_left_of_destination_slat_page);
        std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_source_physical_address + bytes_read, &size_left_of_source_slat_page);

        std::uint64_t size_left_of_slat_pages = crt::min(size_left_of_source_slat_page, size_left_of_destination_slat_page);

        std::uint64_t copy_size = crt::min(size_left_to_read, size_left_of_slat_pages);

        crt::copy_memory(reinterpret_cast<void*>(host_destination), reinterpret_cast<const void*>(host_source), copy_size);

        size_left_to_read -= copy_size;
        bytes_read += copy_size;
    }

    return bytes_read;
}

void hypercall::process(trap_frame_t* trap_frame)
{
    hypercall_info_t hypercall_info = { .value = trap_frame->rcx };

    switch (hypercall_info.call_type)
    {
    case hypercall_type_t::read_guest_physical_memory:
    {
        trap_frame->rax = read_guest_physical_memory(trap_frame);

        break;
    }
    case hypercall_type_t::null:
    default:
        break;
    }

    arch::advance_rip();
}
