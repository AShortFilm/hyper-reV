#include "hypercall.h"
#include <hypercall/hypercall_def.h>

extern "C" std::uint64_t launch_raw_hypercall(hypercall_info_t rcx, std::uint64_t rdx, std::uint64_t r8, std::uint64_t r9);

std::uint64_t make_hypercall(hypercall_type_t call_type, std::uint64_t call_reserved_data, std::uint64_t rdx, std::uint64_t r8, std::uint64_t r9)
{
	hypercall_info_t hypercall_info = { };

	hypercall_info.key = 0x4E47;
	hypercall_info.call_type = call_type;
	hypercall_info.call_reserved_data = call_reserved_data;

	return launch_raw_hypercall(hypercall_info, rdx, r8, r9);
}

std::uint64_t hypercall::read_guest_physical_memory(void* guest_virtual_read_buffer, std::uint64_t guest_physical_address, std::uint64_t size)
{
	hypercall_type_t call_type = hypercall_type_t::read_guest_physical_memory;

	std::uint64_t read_buffer = reinterpret_cast<std::uint64_t>(guest_virtual_read_buffer);

	return make_hypercall(call_type, 0, guest_physical_address, read_buffer, size);
}
