#include "system.h"

#include <algorithm>

#include "../hypercall/hypercall.h"
#include "../hook/hook.h"

#include <portable_executable/image.hpp>

#include <print>
#include <vector>
#include <Windows.h>
#include <winternl.h>
#include <intrin.h>

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread, std::uint8_t* previous_enabled_state);

std::vector<std::uint8_t> dump_kernel_module(std::uint64_t module_base_address)
{
	constexpr std::uint64_t headers_size = 0x1000;

	std::vector<std::uint8_t> headers(headers_size);

	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(headers.data(), module_base_address, sys::current_cr3, headers_size);

	if (bytes_read != headers_size)
	{
		return { };
	}

	std::uint16_t magic = *reinterpret_cast<std::uint16_t*>(headers.data());

	if (magic != 0x5a4d)
	{
		return { };
	}

	const portable_executable::image_t* image = reinterpret_cast<portable_executable::image_t*>(headers.data());

	std::vector<std::uint8_t> image_buffer(image->nt_headers()->optional_header.size_of_image);

	memcpy(image_buffer.data(), headers.data(), 0x1000);

	for (const auto& current_section : image->sections())
	{
		std::uint64_t read_offset = current_section.virtual_address;
		std::uint64_t read_size = current_section.virtual_size;

		hypercall::read_guest_virtual_memory(image_buffer.data() + read_offset, module_base_address + read_offset, sys::current_cr3, read_size);
	}

	return image_buffer;
}

std::uint64_t find_kernel_detour_holder_base_address(portable_executable::image_t* ntoskrnl, std::uint64_t ntoskrnl_base_address)
{
	for (const auto& current_section : ntoskrnl->sections())
	{
		std::string_view current_section_name(current_section.name);

		if (current_section_name.contains("Pad") == true && current_section.characteristics.mem_execute == 1)
		{
			return ntoskrnl_base_address + current_section.virtual_address;
		}
	}

	return 0;
}

std::unordered_map<std::string, std::uint64_t> parse_module_exports(const portable_executable::image_t* image, const std::string& module_name, const std::uint64_t module_base_address)
{
	std::unordered_map<std::string, std::uint64_t> exports = { };

	for (const auto& current_export : image->exports())
	{
		std::string current_export_name = module_name + "!" + current_export.name;

		std::uint64_t delta = reinterpret_cast<std::uint64_t>(current_export.address) - image->as<std::uint64_t>();

		exports[current_export_name] = module_base_address + delta;
	}

	return exports;
}

std::uint8_t sys::kernel::parse_modules()
{
	auto loaded_modules = get_loaded_modules();

	if (loaded_modules.size() <= 1)
	{
		return 0;
	}

	for (const rtl_process_module_information_t& current_module : loaded_modules)
	{
		std::string module_name = reinterpret_cast<const char*>(current_module.full_path_name + current_module.offset_to_file_name);

		if (modules_list.contains(module_name) == true)
		{
			const kernel_module_t already_present_module = modules_list[module_name];

			if (already_present_module.base_address == current_module.image_base && already_present_module.size == current_module.image_size)
			{
				continue;
			}
		}

		std::vector<std::uint8_t> module_dump = dump_kernel_module(current_module.image_base);

		if (module_dump.empty() == true)
		{
			continue;
		}

		kernel_module_t kernel_module = { };

		portable_executable::image_t* image = reinterpret_cast<portable_executable::image_t*>(module_dump.data());

		kernel_module.exports = parse_module_exports(image, module_name, current_module.image_base);
		kernel_module.base_address = current_module.image_base;
		kernel_module.size = current_module.image_size;

		modules_list[module_name] = kernel_module;
	}

	return 1;
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

	std::uint64_t ntoskrnl_base_address = ntoskrnl->image_base;

	if (ntoskrnl_base_address == 0)
	{
		std::println("unable to find ntoskrn's base address");

		return 0;
	}

	std::vector<std::uint8_t> ntoskrnl_dump = dump_kernel_module(ntoskrnl_base_address);

	if (ntoskrnl_dump.empty() == true)
	{
		std::println("unable to dump ntoskrnl");

		return 0;
	}

	portable_executable::image_t* ntoskrnl_image = reinterpret_cast<portable_executable::image_t*>(ntoskrnl_dump.data());

	hook::kernel_detour_holder_base = find_kernel_detour_holder_base_address(ntoskrnl_image, ntoskrnl_base_address);

	if (hook::kernel_detour_holder_base == 0)
	{
		std::println("unable to locate kernel hook holder");

		return 0;
	}

	if (kernel::parse_modules() == 0)
	{
		std::println("unable to parse kernel modules");

		return 0;
	}

	if (hook::set_up() == 0)
	{
		std::println("unable to set up kernel hook helper");

		return 0;
	}

	return 1;
}

void sys::clean_up()
{
	hook::clean_up();
}

std::uint8_t sys::acquire_privilege()
{
	constexpr std::uint32_t debug_privilege_id = 20;

	std::uint8_t previous_state = 0;

	std::uint32_t status = user::adjust_privilege(debug_privilege_id, 1, 0, &previous_state);

	return NT_SUCCESS(status);
}

std::vector<rtl_process_module_information_t> sys::kernel::get_loaded_modules()
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

	rtl_process_module_information_t* start = &process_modules->modules[0];
	rtl_process_module_information_t* end = start + process_modules->module_count;

	return { start, end };
}

std::optional<rtl_process_module_information_t> sys::kernel::get_module_information(std::string_view target_module_name)
{
	const std::vector<rtl_process_module_information_t> loaded_modules = get_loaded_modules();

	for (const rtl_process_module_information_t& current_module : loaded_modules)
	{
		std::string_view current_module_name = reinterpret_cast<const char*>(current_module.full_path_name + current_module.offset_to_file_name);

		if (target_module_name == current_module_name)
		{
			return current_module;
		}
	}

	return std::nullopt;
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

std::string sys::user::to_string(const std::wstring& wstring)
{
	std::string converted_string = { };

	std::ranges::transform(wstring,
		std::back_inserter(converted_string), [](wchar_t character)
		{
			return static_cast<char>(character);
		});

	return converted_string;
}
