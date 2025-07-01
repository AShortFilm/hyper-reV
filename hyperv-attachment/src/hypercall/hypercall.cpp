#include "hypercall.h"
#include <hypercall/hypercall_def.h>
#include "../memory_manager/memory_manager.h"
#include "../slat/slat.h"
#include "../arch/arch.h"
#include "../logs/logs.h"
#include "../crt/crt.h"
#include "../memory_manager/heap_manager.h"

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

    std::uint64_t bytes_copied = 0;

    while (size_left_to_read != 0)
    {
        std::uint64_t size_left_of_destination_virtual_page = 0;
        std::uint64_t size_left_of_destination_slat_page = 0;

        std::uint64_t size_left_of_source_virtual_page = 0;
        std::uint64_t size_left_of_source_slat_page = 0;

        std::uint64_t guest_source_physical_address = memory_manager::translate_guest_virtual_address(guest_source_cr3, slat_cr3, { .address = guest_source_virtual_address + bytes_copied }, &size_left_of_source_virtual_page);
        std::uint64_t guest_destination_physical_address = memory_manager::translate_guest_virtual_address(guest_destination_cr3, slat_cr3, { .address = guest_destination_virtual_address + bytes_copied }, &size_left_of_destination_virtual_page);

        if (guest_source_physical_address == 0 || guest_destination_physical_address == 0)
        {
            break;
        }

        std::uint64_t host_destination = memory_manager::map_guest_physical(slat_cr3, guest_destination_physical_address, &size_left_of_destination_slat_page);
        std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_source_physical_address, &size_left_of_source_slat_page);

        if (operation == memory_operation_t::write_operation)
        {
            crt::swap(host_source, host_destination);
        }

        std::uint64_t size_left_of_slat_pages = crt::min(size_left_of_source_slat_page, size_left_of_destination_slat_page);
        std::uint64_t size_left_of_virtual_pages = crt::min(size_left_of_source_virtual_page, size_left_of_destination_virtual_page);

        std::uint64_t size_left_of_pages = crt::min(size_left_of_slat_pages, size_left_of_virtual_pages);

        std::uint64_t copy_size = crt::min(size_left_to_read, size_left_of_pages);

        crt::copy_memory(reinterpret_cast<void*>(host_destination), reinterpret_cast<const void*>(host_source), copy_size);

        size_left_to_read -= copy_size;
        bytes_copied += copy_size;
    }

    return bytes_copied;
}

struct rsp_stored_info_t
{
    std::uint64_t rcx;
    std::uint64_t return_address;
};

rsp_stored_info_t read_rsp_stored_info_from_log_exit(cr3 guest_cr3, std::uint64_t rsp)
{
    if (rsp == 0)
    {
        return { };
    }

    cr3 slat_cr3 = slat::get_cr3();

    rsp_stored_info_t info = { };

    std::uint64_t bytes_read = 0;
    std::uint64_t bytes_remaining = sizeof(info);

    while (bytes_remaining != 0)
    {
        std::uint64_t virtual_size_left = 0;

        std::uint64_t rsp_guest_physical_address = memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = rsp + bytes_read }, &virtual_size_left);

        if (rsp_guest_physical_address == 0)
        {
            return info;
        }

        std::uint64_t physical_size_left = 0;

        // rcx has just been pushed onto stack
        std::uint64_t* rsp_mapped = reinterpret_cast<std::uint64_t*>(memory_manager::map_guest_physical(slat_cr3, rsp_guest_physical_address, &physical_size_left));

        std::uint64_t size_left_of_page = crt::min(physical_size_left, virtual_size_left);
        std::uint64_t size_to_read = crt::min(bytes_remaining, size_left_of_page);

        crt::copy_memory(reinterpret_cast<std::uint8_t*>(&info) + bytes_read, reinterpret_cast<std::uint8_t*>(rsp_mapped) + bytes_read, size_to_read);

        bytes_remaining -= size_to_read;
        bytes_read += size_to_read;
    }

    return info;
}

void log_current_state(trap_frame_log_t trap_frame)
{
    trap_frame.rip = arch::get_guest_rip();

    cr3 guest_cr3 = arch::get_guest_cr3();

    rsp_stored_info_t rsp_stored_info = read_rsp_stored_info_from_log_exit(guest_cr3, trap_frame.rsp);

    trap_frame.rcx = rsp_stored_info.rcx;
    trap_frame.cr3 = guest_cr3.flags;
    trap_frame.potential_return_address = rsp_stored_info.return_address;

    logs::add_log(trap_frame);
}

std::uint64_t flush_logs(trap_frame_t* trap_frame)
{
    std::uint64_t stored_logs_count = logs::stored_log_index;

    cr3 guest_cr3 = arch::get_guest_cr3();
    cr3 slat_cr3 = slat::get_cr3();

    std::uint64_t guest_virtual_address = trap_frame->rdx;
    std::uint16_t count = static_cast<std::uint16_t>(trap_frame->r8);

    if (logs::flush(slat_cr3, guest_virtual_address, guest_cr3, count) == 0)
    {
        return -1;
    }

    return stored_logs_count;
}

void hypercall::process(hypercall_info_t hypercall_info, trap_frame_t* trap_frame)
{
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
    case hypercall_type_t::read_guest_cr3:
    {
        cr3 guest_cr3 = arch::get_guest_cr3();

        trap_frame->rax = guest_cr3.flags;

        break;
    }
    case hypercall_type_t::add_slat_code_hook:
    {
        virtual_address_t target_guest_physical_address = { .address = trap_frame->rdx };
        virtual_address_t shadow_page_guest_physical_address = { .address = trap_frame->r8 };

        cr3 slat_cr3 = slat::get_cr3();

        trap_frame->rax = slat::add_slat_code_hook(slat_cr3, target_guest_physical_address, shadow_page_guest_physical_address);

        break;
    }
    case hypercall_type_t::remove_slat_code_hook:
    {
        virtual_address_t target_guest_physical_address = { .address = trap_frame->rdx };

        cr3 slat_cr3 = slat::get_cr3();

        trap_frame->rax = slat::remove_slat_code_hook(slat_cr3, target_guest_physical_address);

        break;
    }
    case hypercall_type_t::hide_guest_physical_page:
    {
        virtual_address_t target_guest_physical_address = { .address = trap_frame->rdx };

        cr3 slat_cr3 = slat::get_cr3();

        trap_frame->rax = slat::hide_physical_page_from_guest(slat_cr3, target_guest_physical_address);

        break;
    }
    case hypercall_type_t::log_current_state:
    {
        trap_frame_log_t trap_frame_log = { };

        crt::copy_memory(&trap_frame_log, trap_frame, sizeof(trap_frame_t));

        log_current_state(trap_frame_log);

        break;
    }
    case hypercall_type_t::flush_logs:
    {
        trap_frame->rax = flush_logs(trap_frame);

        break;
    }
    case hypercall_type_t::get_heap_free_page_count:
    {
        trap_frame->rax = heap_manager::get_free_page_count();

        break;
    }
    default:
        break;
    }

    arch::advance_guest_rip();
}
