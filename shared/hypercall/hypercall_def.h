#pragma once
#include <cstdint>

enum class hypercall_type_t : std::uint64_t
{
    null,
    guest_physical_memory_operation,
    guest_virtual_memory_operation,
    translate_guest_virtual_address,
    read_guest_cr3,
    add_slat_code_hook,
    remove_slat_code_hook
};

enum class memory_operation_t : std::uint64_t
{
    read_operation,
    write_operation
};

#pragma warning(push)
#pragma warning(disable: 4201)

union hypercall_info_t
{
    std::uint64_t value;

    struct
    {
        std::uint64_t key : 16;
        hypercall_type_t call_type : 6;
        std::uint64_t call_reserved_data : 42;
    };
};

union virt_memory_op_hypercall_info_t
{
    std::uint64_t value;

    struct
    {
        std::uint64_t key : 16;
        hypercall_type_t call_type : 6;
        memory_operation_t memory_operation : 1;
        std::uint64_t address_of_page_directory : 36; // we will construct the other cr3 (aside from the caller process) involved in the operation from this
        std::uint64_t reserved : 5;
    };
};

#pragma warning(pop)
