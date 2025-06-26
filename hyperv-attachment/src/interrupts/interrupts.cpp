#include "interrupts.h"
#include "../slat/slat.h"
#include "../crt/crt.h"

#include "ia32-doc/ia32.hpp"

#include <intrin.h>

extern "C"
{
    std::uint64_t original_nmi_handler = 0;

    void nmi_standalone_entry();
    void nmi_entry();
}

namespace
{
    inline std::uint64_t processor_nmi_states[512] = { };
}

void set_up_nmi_handling()
{
    segment_descriptor_register_64 idtr = { };

    __sidt(&idtr);

    if (idtr.base_address == 0)
    {
        return;
    }

    segment_descriptor_interrupt_gate_64* interrupt_gates = reinterpret_cast<segment_descriptor_interrupt_gate_64*>(idtr.base_address);
    segment_descriptor_interrupt_gate_64* nmi_gate = &interrupt_gates[2];
    segment_descriptor_interrupt_gate_64 new_gate = *nmi_gate;

    std::uint64_t new_handler = reinterpret_cast<std::uint64_t>(nmi_entry);

    if (new_gate.present == 0)
    {
        segment_selector gate_segment_selector = { .index = 1 };

        new_gate.segment_selector = gate_segment_selector.flags;
        new_gate.type = SEGMENT_DESCRIPTOR_TYPE_INTERRUPT_GATE;
        new_gate.present = 1;

        new_handler = reinterpret_cast<std::uint64_t>(nmi_standalone_entry);
    }
    else
    {
        original_nmi_handler = nmi_gate->offset_low | (nmi_gate->offset_middle << 16) | (static_cast<uint64_t>(nmi_gate->offset_high) << 32);
    }

    new_gate.offset_low = new_handler & 0xFFFF;
    new_gate.offset_middle = (new_handler >> 16) & 0xFFFF;
    new_gate.offset_high = (new_handler >> 32) & 0xFFFFFFFF;

    *nmi_gate = new_gate;
}

void interrupts::set_up()
{
	apic = apic_t::create_instance();

#ifdef _INTELMACHINE
    set_up_nmi_handling();
#endif
}

void interrupts::set_all_nmi_ready()
{
	for (std::uint64_t& row_state : processor_nmi_states)
	{
        row_state = UINT64_MAX;
	}
}

std::uint64_t* get_processor_nmi_state_row(std::uint64_t apic_id)
{
    const std::uint64_t row_id = apic_id / 64;

    return &processor_nmi_states[row_id];
}

void interrupts::set_nmi_ready(std::uint64_t apic_id)
{
    std::uint64_t* row = get_processor_nmi_state_row(apic_id);
    const std::uint64_t bit = apic_id % 64;

    *row |= 1ull << bit;
}

void interrupts::clear_nmi_ready(std::uint64_t apic_id)
{
    std::uint64_t* row = get_processor_nmi_state_row(apic_id);
    const std::uint64_t bit = apic_id % 64;

    *row &= ~(1ull << bit);
}

std::uint8_t interrupts::is_nmi_ready(std::uint64_t apic_id)
{
    const std::uint64_t row = *get_processor_nmi_state_row(apic_id);
    const std::uint64_t bit = apic_id % 64;

    return (row >> bit) & 1;
}

void interrupts::process_nmi()
{
    const std::uint64_t current_apic_id = apic_t::current_apic_id();

	if (is_nmi_ready(current_apic_id) == 1)
    {
        slat::flush_current_logical_processor_slat_cache();

        clear_nmi_ready(current_apic_id);
    }
}

void interrupts::send_nmi_all_but_self()
{
    apic->send_nmi(icr_destination_shorthand_t::all_but_self);
}
