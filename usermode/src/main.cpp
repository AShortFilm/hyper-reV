#include <Windows.h>
#include <iostream>
#include <print>

#include "hypercall/hypercall.h"

std::int32_t main()
{
	std::uint64_t guest_target_physical_address = 0;

	std::print("enter target page guest physical address (hexadecimal): ");
	std::cin >> std::hex >> guest_target_physical_address;

	std::uint64_t shadow_page_guest_physical_address = 0;

	std::print("enter shadow page guest physical address (hexadecimal): ");
	std::cin >> std::hex >> shadow_page_guest_physical_address;

	std::uint64_t hypercall_response = hypercall::add_slat_code_hook(guest_target_physical_address, shadow_page_guest_physical_address);

	std::println("hypercall response: 0x{:x}", hypercall_response);

	std::system("pause");

	return 0;
}
