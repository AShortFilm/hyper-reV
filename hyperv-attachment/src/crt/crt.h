#pragma once
#include <cstdint>

namespace crt
{
	void copy_memory(void* destination, const void* source, std::uint64_t size);
	void set_memory(void* destination, std::uint8_t value, std::uint64_t size);

	template <class t>
	t min(t a, t b)
	{
		return (a < b) ? a : b;
	}

	template <class t>
	t max(t a, t b)
	{
		return (a < b) ? b : a;
	}

	template <class t>
	t abs(t n)
	{
		return (n < 0) ? -n : n;
	}

	template <class t>
	void swap(t& a, t& b)
	{
		t cache = a;

		a = b;
		b = cache;
	}

	class mutex_t
	{
	protected:
		volatile std::int64_t value;

	public:
		mutex_t();
		
		void lock();
		void release();
	};
}
