#include "crt.h"
#include <intrin.h>

void crt::copy_memory(void* destination, const void* source, std::uint64_t size)
{
	__movsb(static_cast<std::uint8_t*>(destination), static_cast<const std::uint8_t*>(source), size);
}

void crt::set_memory(void* destination, std::uint8_t value, std::uint64_t size)
{
	__stosb(static_cast<std::uint8_t*>(destination), value, size);
}

crt::mutex_t::mutex_t() : value(0)
{
	
}

void crt::mutex_t::lock()
{
	while (_InterlockedCompareExchange64(&this->value, 1, 0) != 0)
	{
		_mm_pause();
	}
}

void crt::mutex_t::release()
{
	_InterlockedExchange64(&this->value, 0);
}
