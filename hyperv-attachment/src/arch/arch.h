#pragma once
#include <ia32-doc/ia32.hpp>
#include <cstdint>

namespace arch
{
	std::uint64_t get_vmexit_reason();
	std::uint8_t is_cpuid(std::uint64_t vmexit_reason);
	std::uint8_t is_slat_violation(std::uint64_t vmexit_reason);

	cr3 get_guest_cr3();

	void advance_rip();
}
