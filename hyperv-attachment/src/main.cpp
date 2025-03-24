#include "arch/arch.h"
#include "slat/slat.h"
#include <ia32-doc/ia32.hpp>
#include <intrin.h>
#include <cstdint>

std::uint8_t* original_vmexit_handler = NULL;

struct trap_frame_t
{
    std::uint64_t rax;
    std::uint64_t rcx;
    std::uint64_t rdx;
};

typedef std::uint64_t(*vmexit_handler_t)(trap_frame_t** a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4);

std::uint64_t vmexit_handler_detour(trap_frame_t** a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4)
{
    trap_frame_t* trap_frame = *a1;

    std::uint64_t exit_reason = arch::get_vmexit_reason();

    if (arch::is_cpuid(exit_reason) == 1 && trap_frame->rcx == 1337)
    {
        virtual_address_t guest_physical_address = { 0 };

        guest_physical_address.address = trap_frame->rdx;

        trap_frame->rax = slat::translate_guest_physical_address(slat::get_cr3(), guest_physical_address);

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
