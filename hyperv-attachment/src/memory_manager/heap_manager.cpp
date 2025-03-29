#include "heap_manager.h"
#include "../crt/crt.h"
#include <intrin.h>

namespace heap_manager
{
	crt::mutex_t allocation_mutex = { };
}

void heap_manager::set_up(std::uint64_t heap_base, std::uint64_t heap_size)
{
	list_head = reinterpret_cast<heap_entry_t*>(heap_base);

	list_head->size = heap_size - sizeof(heap_entry_t);
	list_head->is_allocated = 0;

	heap_end = heap_base + heap_size;
}

void* heap_manager::allocate_memory(std::uint32_t size)
{
	allocation_mutex.lock();

	heap_entry_t* entry = list_head;

	while (entry != nullptr)
	{
		if (entry->is_allocated == 0)
		{
			if (entry->size == size)
			{
				entry->is_allocated = 1;

				allocation_mutex.release();

				return ++entry;
			}
			else
			{
				heap_entry_t* split_entry = entry->split(size);

				if (split_entry != nullptr)
				{
					allocation_mutex.release();

					return ++split_entry;
				}
			}
		}

		entry = entry->get_next();
	}

	allocation_mutex.release();

	return nullptr;
}

void heap_manager::free_memory(void* allocation_base)
{
	if (allocation_base == nullptr)
	{
		return;
	}

	heap_entry_t* entry = reinterpret_cast<heap_entry_t*>(allocation_base) - 1;

	entry->is_allocated = 0;
}

heap_manager::heap_entry_t* heap_manager::heap_entry_t::get_next()
{
	std::uint64_t next_entry = reinterpret_cast<std::uint64_t>(this + 1) + this->size;

	if (heap_end <= next_entry)
	{
		return nullptr;
	}

	return reinterpret_cast<heap_entry_t*>(next_entry);
}

// note for future: if we ever need any size above 0x1000 (at the time of writing, we do not)
// then we will start changing the shrink direction by the size
// if the allocation is less than or equal to 4kb, then it will be shrank from 1 direction,
// if its bigger than 4kb, then it will shrink from the other direction
// this will allow us to manage the memory better, so when we search for deallocated entries of the same size,
// we wont have to search as far
heap_manager::heap_entry_t* heap_manager::heap_entry_t::split(std::uint32_t size_of_next_entry)
{
	std::uint32_t needed_size = size_of_next_entry + sizeof(heap_entry_t);

	if (this->is_allocated == 1 || this->size <= needed_size)
	{
		return nullptr;
	}

	this->size -= needed_size;

	heap_entry_t* next_entry = this->get_next();

	next_entry->is_allocated = 1;
	next_entry->size = size_of_next_entry;

	return next_entry;
}
