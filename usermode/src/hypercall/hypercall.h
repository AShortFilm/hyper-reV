#pragma once
#include <cstdint>

namespace hypercall
{
	std::uint64_t read_guest_physical_memory(void* guest_destination_buffer, std::uint64_t guest_source_physical_address, std::uint64_t size);
	std::uint64_t write_guest_physical_memory(void* guest_source_buffer, std::uint64_t guest_destination_physical_address, std::uint64_t size);

	std::uint64_t read_guest_virtual_memory(void* guest_destination_buffer, std::uint64_t guest_source_virtual_address, std::uint64_t source_cr3, std::uint64_t size);
	std::uint64_t write_guest_virtual_memory(void* guest_source_buffer, std::uint64_t guest_destination_virtual_address, std::uint64_t destination_cr3, std::uint64_t size);

	std::uint64_t translate_guest_virtual_address(std::uint64_t guest_virtual_address, std::uint64_t guest_cr3);
}
