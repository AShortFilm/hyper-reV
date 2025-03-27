#pragma once
#include <cstdint>

namespace crt
{
	void copy_memory(void* destination, const void* source, std::uint64_t size);

	template <class t>
	t min(t a, t b)
	{
		return (a < b) ? a : b;
	}
}
