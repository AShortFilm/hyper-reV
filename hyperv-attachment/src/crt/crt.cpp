#include "crt.h"
#include <intrin.h>

void crt::copy_memory(void* destination, const void* source, std::uint64_t size)
{
	__movsb(reinterpret_cast<std::uint8_t*>(destination), reinterpret_cast<const std::uint8_t*>(source), size);
}

crt::mutex_t::mutex_t() : value(0)
{
	
}

void crt::mutex_t::lock()
{
	while (_InterlockedCompareExchange64(&this->value, 1, 0) == 0)
	{
		_mm_pause();
	}
}

void crt::mutex_t::release()
{
	this->value = 0;
}
