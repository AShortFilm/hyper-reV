#include "memory_manager.h"
#include "../structures/virtual_address.h"

std::uint64_t memory_manager::map_host_physical(std::uint64_t physical_address)
{
	constexpr std::uint64_t physical_memory_access_base = 255ull << 39;

	return physical_memory_access_base + physical_address;
}
