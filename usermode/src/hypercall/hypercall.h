#pragma once
#include <cstdint>

namespace hypercall
{
	std::uint64_t read_guest_physical_memory(void* guest_virtual_read_buffer, std::uint64_t guest_physical_address, std::uint64_t size);
}
