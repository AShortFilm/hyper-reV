#include <iostream>
#include <print>

extern "C" std::uint64_t do_hv_call(std::uint64_t rcx, std::uint64_t rdx);

std::int32_t main()
{
	std::uint64_t guest_physical_address = 0;

	std::print("enter guest physical address to read: ");
	std::cin >> std::hex >> guest_physical_address;

	std::uint64_t hv_call_response = do_hv_call(1337, guest_physical_address);

	std::println("guest physical address 0x{:x} translates to host physical address 0x{:x}", guest_physical_address, hv_call_response);

	std::system("pause");

	return 0;
}
