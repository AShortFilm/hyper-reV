#include <iostream>
#include <print>

#include "hypercall/hypercall.h"
#include <hypercall/hypercall_def.h>

std::int32_t main()
{
	std::uint64_t guest_physical_address = 0;

	std::print("enter guest physical address to operate on qword from (hexadecimal): ");
	std::cin >> std::hex >> guest_physical_address;

	std::int8_t target_operation = 0;

	std::print("what operation is this (r/w): ");
	std::cin >> target_operation;

	if (target_operation == 'r')
	{
		std::uint64_t read_buffer = 0;

		std::uint64_t hypercall_response = hypercall::read_guest_physical_memory(&read_buffer, guest_physical_address, sizeof(read_buffer));

		std::println("hypercall response: 0x{:x}, read value: 0x{:x}", hypercall_response, read_buffer);
	}
	else if (target_operation == 'w')
	{
		std::uint64_t write_buffer = 0;

		std::print("enter value to write to the address (hexadecimal): ");
		std::cin >> std::hex >> write_buffer;

		std::uint64_t hypercall_response = hypercall::write_guest_physical_memory(&write_buffer, guest_physical_address, sizeof(write_buffer));

		std::println("hypercall response: 0x{:x}", hypercall_response);
	}

	std::system("pause");

	return 0;
}
