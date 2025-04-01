#pragma once
#include <cstdint>

namespace heap_manager
{
	void set_up(std::uint64_t heap_base, std::uint64_t heap_size);

	void* allocate_page();
	void free_memory(void* pointer);

	struct heap_entry_t
	{
		heap_entry_t* next = nullptr;
	};
}
