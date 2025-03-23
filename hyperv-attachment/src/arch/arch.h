#pragma once
#include <cstdint>

namespace arch
{
	std::uint64_t get_vmexit_reason();
	std::uint8_t is_cpuid(std::uint64_t vmexit_reason);

	void advance_rip();
}
