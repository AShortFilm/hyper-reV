#include <iostream>
#include <print>

#include <hypercall/hypercall_def.h>

extern "C" std::uint64_t do_hv_call(hypercall_info_t rcx, std::uint64_t rdx, void* r8, std::uint64_t r9);

std::int32_t main()
{
	std::uint64_t guest_physical_address = 0;

	std::print("enter guest physical address to read qword from: ");
	std::cin >> std::hex >> guest_physical_address;

	hypercall_info_t hypercall_info = { };

	hypercall_info.key = 0x4E47;
	hypercall_info.call_type = hypercall_type_t::read_guest_physical_memory;

	std::uint64_t read_buffer = 0;

	std::uint64_t hv_call_response = do_hv_call(hypercall_info, guest_physical_address, &read_buffer, sizeof(read_buffer));

	std::println("hv call response: 0x{:x}, read value: 0x{:x}", hv_call_response, read_buffer);

	std::system("pause");

	return 0;
}
