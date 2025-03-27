#pragma once
#include <cstdint>

namespace hypercall
{
	std::uint64_t read_guest_physical_memory(void* guest_virtual_buffer, std::uint64_t guest_physical_address, std::uint64_t size);
	std::uint64_t write_guest_physical_memory(void* guest_virtual_buffer, std::uint64_t guest_physical_address, std::uint64_t size);
}
