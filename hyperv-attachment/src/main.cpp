#include "arch/arch.h"
#include "hypercall/hypercall.h"
#include "hypercall/hypercall_def.h"
#include "memory_manager/memory_manager.h"
#include "memory_manager/heap_manager.h"
#include "slat/slat.h"
#include "logs/logs.h"
#include "structures/trap_frame.h"
#include <ia32-doc/ia32.hpp>
#include <intrin.h>
#include <cstdint>

#include "interrupts/interrupts.h"

std::uint8_t* original_vmexit_handler = NULL;

typedef std::uint64_t(*vmexit_handler_t)(trap_frame_t** a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4);

void process_first_vmexit()
{
    static std::uint8_t is_first_vmexit = 1;

    if (is_first_vmexit == 1)
    {
        interrupts::set_up();

        is_first_vmexit = 0;
    }
}

std::uint64_t vmexit_handler_detour(trap_frame_t** a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4)
{
    process_first_vmexit();
    
    trap_frame_t* trap_frame = *a1;

    std::uint64_t exit_reason = arch::get_vmexit_reason();

    if (arch::is_cpuid(exit_reason) == 1)
    {
        hypercall_info_t hypercall_info = { .value = trap_frame->rcx };

        if (hypercall_info.primary_key == hypercall_primary_key && hypercall_info.secondary_key == hypercall_secondary_key)
        {
            hypercall::process(hypercall_info, trap_frame);

            return 0;
        }
    }
    else if (arch::is_slat_violation(exit_reason) == 1 && slat::process_slat_violation() == 1)
    {
        return 0;
    }
    else if (arch::is_interrupt_exit(exit_reason) == 1 && arch::is_exit_interrupt_non_maskable() == 1)
    {
        interrupts::process_nmi();
    }

    return reinterpret_cast<vmexit_handler_t>(original_vmexit_handler)(a1, a2, a3, a4);
}

void entry_point(std::uint8_t** vmexit_handler_detour_out, std::uint8_t* original_vmexit_handler_routine, std::uint64_t heap_physical_base, std::uint64_t heap_size)
{
    original_vmexit_handler = original_vmexit_handler_routine;

    *vmexit_handler_detour_out = reinterpret_cast<std::uint8_t*>(vmexit_handler_detour);

    std::uint64_t mapped_heap_base = memory_manager::map_host_physical(heap_physical_base);
  
    heap_manager::set_up(mapped_heap_base, heap_size);

    slat::set_up();
    logs::set_up();
}
