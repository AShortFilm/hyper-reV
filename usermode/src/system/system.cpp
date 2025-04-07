#include "system.h"
#include "../hypercall/hypercall.h"
#include "../hook/hook.h"

#include <portable_executable/image.hpp>

#include <print>
#include <vector>
#include <Windows.h>
#include <winternl.h>

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread, std::uint8_t* previous_enabled_state);

std::uint64_t find_kernel_hook_holder_base_address()
{
	constexpr std::uint64_t headers_size = 0x1000;

	std::vector<std::uint8_t> buffer(headers_size);

	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(buffer.data(), sys::ntoskrnl_base_address, sys::current_cr3, headers_size);

	if (bytes_read != headers_size)
	{
		return 0;
	}

	portable_executable::image_t* ntoskrnl = reinterpret_cast<portable_executable::image_t*>(buffer.data());

	for (const auto& current_section : ntoskrnl->sections())
	{
		std::string_view current_section_name(current_section.name);

		if (current_section_name.contains("INITKDBG") == true)
		{
			return sys::ntoskrnl_base_address + current_section.virtual_address;
		}
	}

	return 0;
}

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

	if (!ntoskrnl)
	{
		std::println("unable to locate ntoskrnl");

		return 0;
	}

	ntoskrnl_base_address = ntoskrnl->image_base;

	if (ntoskrnl_base_address == 0)
	{
		std::println("unable to read ntoskrnl's base address");

		return 0;
	}

	hook::kernel_hook_holder_base = find_kernel_hook_holder_base_address();

	if (hook::kernel_hook_holder_base == 0)
	{
		std::println("unable to locate kernel hook holder");

		return 0;
	}

	return hook::set_up();
}

std::uint8_t sys::acquire_privilege()
{
	constexpr std::uint32_t debug_privilege_id = 20;

	std::uint8_t previous_state = 0;

	std::uint32_t status = user::adjust_privilege(debug_privilege_id, 1, 0, &previous_state);

	return NT_SUCCESS(status);
}

std::optional<rtl_process_module_information_t> sys::kernel::get_module_information(std::string_view target_module_name)
{
	std::uint32_t size_of_information = 0;

	user::query_system_information(11, nullptr, 0, &size_of_information);

	if (size_of_information == 0)
	{
		return { };
	}

	std::vector<std::uint8_t> buffer(size_of_information);

	std::uint32_t status = user::query_system_information(11, buffer.data(), size_of_information, &size_of_information);

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

std::uint32_t sys::user::query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size, std::uint32_t* returned_size)
{
	return NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(information_class), information_out, information_size, reinterpret_cast<ULONG*>(returned_size));
}

std::uint32_t sys::user::adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only, std::uint8_t* previous_enabled_state)
{
	return RtlAdjustPrivilege(privilege, enable, current_thread_only, previous_enabled_state);
}

void* sys::user::allocate_locked_memory(std::uint64_t size, std::uint32_t protection)
{
	void* allocation_base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protection);

	if (allocation_base == nullptr)
	{
		return 0;
	}

	std::int32_t lock_status = VirtualLock(allocation_base, size);

	if (lock_status == 0)
	{
		free_memory(allocation_base);

		return 0;
	}

	return allocation_base;
}

std::uint8_t sys::user::free_memory(void* address)
{
	std::int32_t free_status = VirtualFree(address, 0, MEM_RELEASE);

	return free_status != 0;
}
