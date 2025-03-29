#pragma once
#include <cstdint>

namespace heap_manager
{
	void set_up(std::uint64_t heap_base, std::uint64_t heap_size);

	void* allocate_memory(std::uint32_t size);
	void free_memory(void* pointer);

	union heap_entry_t
	{
		std::uint32_t value;

		struct
		{
			std::uint32_t size : 31;
			std::uint32_t is_allocated : 1;
		};

		heap_entry_t* get_next();
		heap_entry_t* split(std::uint32_t size_of_next_entry);
	};

	inline heap_manager::heap_entry_t* list_head = 0;
	inline std::uint64_t heap_end = 0;
}
