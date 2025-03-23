#include "arch/arch.h"
#include <ia32-doc/ia32.hpp>
#include <intrin.h>
#include <cstdint>

std::uint8_t* original_vmexit_handler = NULL;

struct trap_frame_t
{
    std::uint64_t rax;
    std::uint64_t rcx;
};

typedef std::uint64_t(*vmexit_handler_t)(trap_frame_t** a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4);

std::uint64_t vmexit_handler_detour(trap_frame_t** a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4)
{
    trap_frame_t* trap_frame = *a1;

    std::uint64_t exit_reason = arch::get_vmexit_reason();

    if (arch::is_cpuid(exit_reason) == 1 && trap_frame->rcx == 1337)
    {
        trap_frame->rax = 1337;

        arch::advance_rip();

        return 0;
    }

    return reinterpret_cast<vmexit_handler_t>(original_vmexit_handler)(a1, a2, a3, a4);
}

void entry_point(std::uint8_t** vmexit_handler_detour_out, std::uint8_t* original_vmexit_handler_routine)
{
    original_vmexit_handler = original_vmexit_handler_routine;

    *vmexit_handler_detour_out = reinterpret_cast<std::uint8_t*>(vmexit_handler_detour);
}
