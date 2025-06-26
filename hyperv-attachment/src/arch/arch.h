#pragma once
#include <ia32-doc/ia32.hpp>
#include <cstdint>

#include "amd_def.h"

namespace arch
{
	std::uint64_t get_vmexit_reason();
	std::uint8_t is_cpuid(std::uint64_t vmexit_reason);
	std::uint8_t is_slat_violation(std::uint64_t vmexit_reason);

	std::uint8_t is_non_maskable_interrupt_exit(std::uint64_t vmexit_reason);

	cr3 get_guest_cr3();

	std::uint64_t get_guest_rip();
	void set_guest_rip(std::uint64_t guest_rip);

	void advance_guest_rip();

#ifndef _INTELMACHINE
	vmcb_t* get_vmcb();
	void parse_vmcb_gadget(const std::uint8_t* get_vmcb_gadget);
#endif
}
