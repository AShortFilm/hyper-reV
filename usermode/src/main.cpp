#include <iostream>
#include <print>

#include "hypercall/hypercall.h"
#include <hypercall/hypercall_def.h>

std::int32_t main()
{
	// 24h2's kernel cr3
	constexpr std::uint64_t kernel_cr3 = 0x1ae000;

	std::uint64_t guest_virtual_address = 0;

	std::print("enter guest kernel virtual address to operate on qword from (hexadecimal): ");
	std::cin >> std::hex >> guest_virtual_address;

	std::int8_t target_operation = 0;

	std::print("what operation is this (r/w): ");
	std::cin >> target_operation;

	if (target_operation == 'r')
	{
		std::uint64_t read_buffer = 0;

		std::uint64_t hypercall_response = hypercall::read_guest_virtual_memory(&read_buffer, guest_virtual_address, kernel_cr3, sizeof(read_buffer));

		std::println("hypercall response: 0x{:x}, read value: 0x{:x}", hypercall_response, read_buffer);
	}
	else if (target_operation == 'w')
	{
		std::uint64_t write_buffer = 0;

		std::print("enter value to write to the address (hexadecimal): ");
		std::cin >> std::hex >> write_buffer;

		std::uint64_t hypercall_response = hypercall::write_guest_virtual_memory(&write_buffer, guest_virtual_address, kernel_cr3, sizeof(write_buffer));

		std::println("hypercall response: 0x{:x}", hypercall_response);
	}

	std::system("pause");

	return 0;
}
