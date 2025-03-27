#include "crt.h"
#include <intrin.h>

void crt::copy_memory(void* destination, const void* source, std::uint64_t size)
{
	__movsb(reinterpret_cast<std::uint8_t*>(destination), reinterpret_cast<const std::uint8_t*>(source), size);
}
