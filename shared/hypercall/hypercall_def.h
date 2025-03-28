#pragma once
#include <cstdint>

enum class hypercall_type_t : std::uint64_t
{
    null,
    guest_physical_memory_operation,
    translate_guest_virtual_address
};

enum class physical_memory_operation_t : std::uint64_t
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

#pragma warning(pop)
