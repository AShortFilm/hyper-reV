#include <iostream>
#include <print>

#include "hypercall/hypercall.h"

std::int32_t main()
{
	std::uint64_t guest_physical_address = 0;

	std::print("enter guest physical address to read qword from: ");
	std::cin >> std::hex >> guest_physical_address;

	std::uint64_t read_buffer = 0;

	std::uint64_t hypercall_response = hypercall::read_guest_physical_memory(&read_buffer, guest_physical_address, sizeof(read_buffer));

	std::println("hypercall response: 0x{:x}, read value: 0x{:x}", hypercall_response, read_buffer);

	std::system("pause");

	return 0;
}
