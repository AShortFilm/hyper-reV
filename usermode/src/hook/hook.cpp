#include "hook.h"
#include "kernel_detour_holder.h"
#include "../system/system.h"
#include "../hypercall/hypercall.h"

#include <Zydis/Zydis.h>

#include <Windows.h>
#include <print>
#include <vector>
#include <array>
#include <memory_resource>

std::uint8_t hook::set_up()
{
	kernel_detour_holder_shadow_page_mapped = static_cast<std::uint8_t*>(sys::user::allocate_locked_memory(0x1000, PAGE_READWRITE));

	if (kernel_detour_holder_shadow_page_mapped == nullptr)
	{
		return 0;
	}

	std::uint64_t shadow_page_physical = hypercall::translate_guest_virtual_address(reinterpret_cast<std::uint64_t>(kernel_detour_holder_shadow_page_mapped), sys::current_cr3);

	if (shadow_page_physical == 0)
	{
		return 0;
	}

	kernel_hook_holder_physical_page = hypercall::translate_guest_virtual_address(kernel_hook_holder_base, sys::current_cr3);

	if (kernel_hook_holder_physical_page == 0)
	{
		return 0;
	}

	// in case of a previously wrongfully ended session which would've left the hook still applied
	hypercall::remove_slat_code_hook(kernel_hook_holder_physical_page);

	std::uint64_t hook_status = hypercall::add_slat_code_hook(kernel_hook_holder_physical_page, shadow_page_physical);

	if (hook_status == 0)
	{
		return 0;
	}

	kernel_detour_holder::set_up(reinterpret_cast<std::uint64_t>(kernel_detour_holder_shadow_page_mapped), 0x1000);

	return 1;
}

union parted_address_t
{
	struct
	{
		std::uint32_t low_part;
		std::uint32_t high_part;
	} u;

	std::uint64_t value;
};

std::vector<std::uint8_t> get_routine_aligned_bytes(std::uint8_t* routine, std::uint8_t minimum_size)
{
	ZydisDecoder decoder = { };

	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
	ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_MINIMAL, ZYAN_TRUE);

	std::vector<std::uint8_t> aligned_bytes = { };

	std::uint8_t* current_instruction = routine;

	while (aligned_bytes.size() < minimum_size)
	{
		ZydisDecodedInstruction decoded_instruction = { };

		ZydisDecoderDecodeInstruction(&decoder, nullptr, current_instruction, ZYDIS_MAX_INSTRUCTION_LENGTH, &decoded_instruction);

		std::uint8_t instruction_length = decoded_instruction.length;

		aligned_bytes.insert(aligned_bytes.end(), current_instruction, current_instruction + instruction_length);

		current_instruction += instruction_length;
	}

	return aligned_bytes;
}

#define d_inline_hook_bytes_size 14

std::vector<std::uint8_t> load_original_bytes_into_shadow_page(std::uint8_t* shadow_page_virtual, std::uint64_t routine_to_hook_physical)
{
	const std::uint64_t page_offset = routine_to_hook_physical & 0xFFF;

	hypercall::read_guest_physical_memory(shadow_page_virtual, routine_to_hook_physical - page_offset, 0x1000);

	return get_routine_aligned_bytes(shadow_page_virtual + page_offset, d_inline_hook_bytes_size);
}

void set_up_inline_hook(std::uint8_t* shadow_page_virtual, std::uint64_t routine_to_hook_physical, std::uint64_t detour_address)
{
	std::array<std::uint8_t, d_inline_hook_bytes_size> inline_hook_bytes = {
		0x68, 0x21, 0x43, 0x65, 0x87, // push   0xffffffff87654321
		0xC7, 0x44, 0x24, 0x04, 0x78, 0x56, 0x34, 0x12, // mov    DWORD PTR [rsp+0x4],0x12345678
		0xC3 // ret
	};

	parted_address_t parted_subroutine_to_jmp_to = { .value = detour_address };

	*reinterpret_cast<std::uint32_t*>(&inline_hook_bytes[1]) = parted_subroutine_to_jmp_to.u.low_part;
	*reinterpret_cast<std::uint32_t*>(&inline_hook_bytes[9]) = parted_subroutine_to_jmp_to.u.high_part;

	const std::uint64_t page_offset = routine_to_hook_physical & 0xFFF;

	memcpy(shadow_page_virtual + page_offset, inline_hook_bytes.data(), sizeof(inline_hook_bytes));
}

std::uint8_t set_up_hook_handler(std::uint64_t routine_to_hook_virtual, std::uint64_t routine_to_hook_physical, std::uint64_t shadow_page_physical, std::vector<std::uint8_t> extra_assembled_bytes, std::uint16_t& detour_holder_shadow_offset, const std::vector<std::uint8_t>& original_bytes)
{
	std::array<std::uint8_t, 14> return_to_original_bytes = {
		0x68, 0x21, 0x43, 0x65, 0x87, // push   0xffffffff87654321
		0xC7, 0x44, 0x24, 0x04, 0x78, 0x56, 0x34, 0x12, // mov    DWORD PTR [rsp+0x4],0x12345678
		0xC3 // ret
	};

	parted_address_t parted_subroutine_to_jmp_to = { .value = routine_to_hook_virtual + original_bytes.size() };

	*reinterpret_cast<std::uint32_t*>(&return_to_original_bytes[1]) = parted_subroutine_to_jmp_to.u.low_part;
	*reinterpret_cast<std::uint32_t*>(&return_to_original_bytes[9]) = parted_subroutine_to_jmp_to.u.high_part;

	std::vector<std::uint8_t> hook_handler_bytes = original_bytes;

	hook_handler_bytes.insert(hook_handler_bytes.end(), extra_assembled_bytes.begin(), extra_assembled_bytes.end());
	hook_handler_bytes.insert(hook_handler_bytes.end(), return_to_original_bytes.begin(), return_to_original_bytes.end());

	void* bytes_buffer = kernel_detour_holder::allocate_memory(static_cast<std::uint16_t>(hook_handler_bytes.size()));

	if (bytes_buffer == nullptr)
	{
		return 0;
	}

	detour_holder_shadow_offset = kernel_detour_holder::get_allocation_offset(bytes_buffer);

	memcpy(bytes_buffer, hook_handler_bytes.data(), hook_handler_bytes.size());

	return 1;
}

std::uint8_t hook::add_kernel_hook(std::uint64_t routine_to_hook_virtual, std::vector<std::uint8_t> extra_assembled_bytes)
{
	if (kernel_hook_list.contains(routine_to_hook_virtual) == true)
	{
		return 0;
	}

	std::uint64_t routine_to_hook_physical = hypercall::translate_guest_virtual_address(routine_to_hook_virtual, sys::current_cr3);

	if (routine_to_hook_physical == 0)
	{
		return 0;
	}

	void* shadow_page_virtual = sys::user::allocate_locked_memory(0x1000, PAGE_READWRITE);

	if (shadow_page_virtual == nullptr)
	{
		return 0;
	}

	std::uint64_t shadow_page_physical = hypercall::translate_guest_virtual_address(reinterpret_cast<std::uint64_t>(shadow_page_virtual), sys::current_cr3);

	if (shadow_page_physical == 0)
	{
		return 0;
	}

	std::vector<std::uint8_t> original_bytes = load_original_bytes_into_shadow_page(static_cast<std::uint8_t*>(shadow_page_virtual), routine_to_hook_physical);

	if (original_bytes.empty() == true)
	{
		return 0;
	}

	std::uint16_t detour_holder_shadow_offset = 0;

	std::uint8_t status = set_up_hook_handler(routine_to_hook_virtual, routine_to_hook_physical, shadow_page_physical, extra_assembled_bytes, detour_holder_shadow_offset, original_bytes);

	if (status == 0)
	{
		return 0;
	}

	std::uint64_t detour_address = kernel_hook_holder_base + detour_holder_shadow_offset;

	set_up_inline_hook(static_cast<std::uint8_t*>(shadow_page_virtual), routine_to_hook_physical, detour_address);

	std::uint64_t hook_status = hypercall::add_slat_code_hook(routine_to_hook_physical, shadow_page_physical);

	if (hook_status == 0)
	{
		return 0;
	}

	kernel_hook_info_t hook_info = { };

	hook_info.set_mapped_shadow_page(shadow_page_virtual);
	hook_info.original_page_physical_address = routine_to_hook_physical;
	hook_info.detour_holder_shadow_offset = detour_holder_shadow_offset;
	
	kernel_hook_list[routine_to_hook_virtual] = hook_info;

	return 1;
}

std::uint8_t hook::remove_kernel_hook(std::uint64_t hooked_routine_virtual)
{
	const auto entry = kernel_hook_list.find(hooked_routine_virtual);

	if (entry == kernel_hook_list.end())
	{
		std::println("unable to find kernel hook");

		return 0;
	}

	kernel_hook_info_t hook_info = entry->second;

	kernel_hook_list.erase(entry);

	if (hypercall::remove_slat_code_hook(hook_info.original_page_physical_address) == 0)
	{
		std::println("unable to remove slat counterpart of kernel hook");

		return 0;
	}

	void* detour_holder_allocation = kernel_detour_holder::get_allocation_from_offset(hook_info.detour_holder_shadow_offset);

	kernel_detour_holder::free_memory(detour_holder_allocation);

	/*if (sys::user::free_memory(hook_info.get_mapped_shadow_page()) == 0)
	{
		std::println("unable to deallocate mapped shadow page");

		return 0;
	}*/

	return 1;
}
