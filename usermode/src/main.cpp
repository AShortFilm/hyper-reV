#include <print>

extern "C" std::uint64_t do_hv_call(std::uint64_t rcx);

std::int32_t main()
{
	std::uint64_t hv_call_response = do_hv_call(1337);

	std::println("hv call response: {}", hv_call_response);

	return 0;
}
