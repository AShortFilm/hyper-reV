#include <cstdint>

std::uint8_t* original_vmexit_handler = NULL;

typedef std::uint64_t(*vmexit_handler_t)(std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4);

std::uint64_t vmexit_handler_detour(std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4)
{
    return reinterpret_cast<vmexit_handler_t>(original_vmexit_handler)(a1, a2, a3, a4);
}

void entry_point(std::uint8_t** vmexit_handler_detour_out, std::uint8_t* original_vmexit_handler_routine)
{
    original_vmexit_handler = original_vmexit_handler_routine;

    *vmexit_handler_detour_out = reinterpret_cast<std::uint8_t*>(vmexit_handler_detour);
}
