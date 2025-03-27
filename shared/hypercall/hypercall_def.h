#pragma once
#include <cstdint>

enum class hypercall_type_t : std::uint64_t
{
    null,
    read_guest_physical_memory
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
