#include "system.h"
#include "../hypercall/hypercall.h"

#include <print>
#include <vector>
#include <Windows.h>
#include <winternl.h>

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread, std::uint8_t* previous_enabled_state);

std::uint8_t sys::set_up()
{
	current_cr3 = hypercall::read_guest_cr3();

	if (current_cr3 == 0)
	{
		std::println("hyperv-attachment doesn't seem to be loaded");

		return 0;
	}

	if (acquire_privilege() == 0)
	{
		std::println("unable to acquire necessary privilege");

		return 0;
	}

	auto ntoskrnl = kernel::get_module_information("ntoskrnl.exe");

	ntoskrnl_base_address = ntoskrnl->image_base;

	if (ntoskrnl_base_address == 0)
	{
		std::println("unable to locate ntoskrnl's base address");

		return 0;
	}

	return 1;
}


std::uint8_t sys::acquire_privilege()
{
	constexpr std::uint32_t debug_privilege_id = 20;

	std::uint8_t previous_state = 0;

	std::uint32_t status = ntdll::adjust_privilege(debug_privilege_id, 1, 0, &previous_state);

	return NT_SUCCESS(status);
}

std::optional<rtl_process_module_information_t> sys::kernel::get_module_information(std::string_view target_module_name)
{
	std::uint32_t size_of_information = 0;

	ntdll::query_system_information(11, nullptr, 0, &size_of_information);

	if (size_of_information == 0)
	{
		return { };
	}

	std::vector<std::uint8_t> buffer(size_of_information);

	std::uint32_t status = ntdll::query_system_information(11, buffer.data(), size_of_information, &size_of_information);

	if (NT_SUCCESS(status) == false)
	{
		return { };
	}

	rtl_process_modules_t* process_modules = reinterpret_cast<rtl_process_modules_t*>(buffer.data());

	for (std::uint64_t i = 0; i < process_modules->module_count; i++)
	{
		rtl_process_module_information_t current_module = process_modules->modules[i];

		std::string_view current_module_name = reinterpret_cast<const char*>(current_module.full_path_name + current_module.offset_to_file_name);

		if (target_module_name == current_module_name)
		{
			return current_module;
		}
	}

	return { };
}

std::uint32_t sys::ntdll::query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size, std::uint32_t* returned_size)
{
	return NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(information_class), information_out, information_size, reinterpret_cast<ULONG*>(returned_size));
}

std::uint32_t sys::ntdll::adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only, std::uint8_t* previous_enabled_state)
{
	return RtlAdjustPrivilege(privilege, enable, current_thread_only, previous_enabled_state);
}
