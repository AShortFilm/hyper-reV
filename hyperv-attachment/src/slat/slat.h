#pragma once
#include <ia32-doc/ia32.hpp>
#include "../structures/virtual_address.h"

namespace slat
{
	cr3 get_cr3();

	std::uint64_t translate_guest_physical_address(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint64_t* size_left_of_slat_page = nullptr);
}
