#include "hypercall.h"
#include <hypercall/hypercall_def.h>
#include "../memory_manager/memory_manager.h"
#include "../slat/slat.h"
#include "../arch/arch.h"
#include "../crt/crt.h"

std::uint64_t operate_on_guest_physical_memory(trap_frame_t* trap_frame, memory_operation_t operation)
{
    cr3 guest_cr3 = arch::get_guest_cr3();
    cr3 slat_cr3 = slat::get_cr3();

    std::uint64_t guest_buffer_virtual_address = trap_frame->r8;
    std::uint64_t guest_physical_address = trap_frame->rdx;

    std::uint64_t size_left_to_read = trap_frame->r9;

    std::uint64_t bytes_read = 0;

    while (size_left_to_read != 0)
    {
        std::uint64_t size_left_of_destination_slat_page = 0;
        std::uint64_t size_left_of_source_slat_page = 0;

        std::uint64_t guest_buffer_physical_address = memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = guest_buffer_virtual_address + bytes_read });

        std::uint64_t host_destination = memory_manager::map_guest_physical(slat_cr3, guest_buffer_physical_address, &size_left_of_destination_slat_page);
        std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_physical_address + bytes_read, &size_left_of_source_slat_page);

        if (operation == memory_operation_t::write_operation)
        {
            crt::swap(host_source, host_destination);
        }

        std::uint64_t size_left_of_slat_pages = crt::min(size_left_of_source_slat_page, size_left_of_destination_slat_page);

        std::uint64_t copy_size = crt::min(size_left_to_read, size_left_of_slat_pages);

        crt::copy_memory(reinterpret_cast<void*>(host_destination), reinterpret_cast<const void*>(host_source), copy_size);

        size_left_to_read -= copy_size;
        bytes_read += copy_size;
    }

    return bytes_read;
}

std::uint64_t operate_on_guest_virtual_memory(trap_frame_t* trap_frame, memory_operation_t operation, std::uint64_t address_of_page_directory)
{
    cr3 guest_source_cr3 = { .address_of_page_directory = address_of_page_directory };
    cr3 guest_destination_cr3 = arch::get_guest_cr3();
    cr3 slat_cr3 = slat::get_cr3();

    std::uint64_t guest_destination_virtual_address = trap_frame->rdx;
    std::uint64_t guest_source_virtual_address = trap_frame->r8;

    std::uint64_t size_left_to_read = trap_frame->r9;

    std::uint64_t bytes_read = 0;

    while (size_left_to_read != 0)
    {
        std::uint64_t size_left_of_destination_slat_page = 0;
        std::uint64_t size_left_of_source_slat_page = 0;

        std::uint64_t guest_source_physical_address = memory_manager::translate_guest_virtual_address(guest_source_cr3, slat_cr3, { .address = guest_source_virtual_address + bytes_read });
        std::uint64_t guest_destination_physical_address = memory_manager::translate_guest_virtual_address(guest_destination_cr3, slat_cr3, { .address = guest_destination_virtual_address + bytes_read });

        std::uint64_t host_destination = memory_manager::map_guest_physical(slat_cr3, guest_destination_physical_address, &size_left_of_destination_slat_page);
        std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_source_physical_address, &size_left_of_source_slat_page);

        if (operation == memory_operation_t::write_operation)
        {
            crt::swap(host_source, host_destination);
        }

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
    case hypercall_type_t::guest_physical_memory_operation:
    {
        memory_operation_t memory_operation = static_cast<memory_operation_t>(hypercall_info.call_reserved_data);

        trap_frame->rax = operate_on_guest_physical_memory(trap_frame, memory_operation);

        break;
    }
    case hypercall_type_t::guest_virtual_memory_operation:
    {
        virt_memory_op_hypercall_info_t virt_memory_op_info = { .value = hypercall_info.value };

        memory_operation_t memory_operation = virt_memory_op_info.memory_operation;
        std::uint64_t address_of_page_directory = virt_memory_op_info.address_of_page_directory;

        trap_frame->rax = operate_on_guest_virtual_memory(trap_frame, memory_operation, address_of_page_directory);

        break;
    }
    case hypercall_type_t::translate_guest_virtual_address:
    {
        virtual_address_t guest_virtual_address = { .address = trap_frame->rdx };

        cr3 target_guest_cr3 = { .flags = trap_frame->r8 };
        cr3 slat_cr3 = slat::get_cr3();

        trap_frame->rax = memory_manager::translate_guest_virtual_address(target_guest_cr3, slat_cr3, guest_virtual_address);

        break;
    }
    case hypercall_type_t::null:
    default:
        break;
    }

    arch::advance_rip();
}
