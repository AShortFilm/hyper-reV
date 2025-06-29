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

	kernel_detour_holder_physical_page = hypercall::translate_guest_virtual_address(kernel_detour_holder_base, sys::current_cr3);

	if (kernel_detour_holder_physical_page == 0)
	{
		return 0;
	}

	// in case of a previously wrongfully ended session which would've left the hook still applied
	hypercall::remove_slat_code_hook(kernel_detour_holder_physical_page);

	hypercall::read_guest_physical_memory(kernel_detour_holder_shadow_page_mapped, kernel_detour_holder_physical_page, 0x1000);

	std::uint64_t hook_status = hypercall::add_slat_code_hook(kernel_detour_holder_physical_page, shadow_page_physical);

	if (hook_status == 0)
	{
		return 0;
	}

	kernel_detour_holder::set_up(reinterpret_cast<std::uint64_t>(kernel_detour_holder_shadow_page_mapped), 0x1000);

	return 1;
}

void hook::clean_up()
{
	for (const auto& [virtual_address, info] : kernel_hook_list)
	{
		remove_kernel_hook(virtual_address);
	}

	if (kernel_detour_holder_physical_page != 0)
	{
		hypercall::remove_slat_code_hook(kernel_detour_holder_physical_page);
	}
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

ZydisDecoder make_zydis_decoder()
{
	ZydisDecoder decoder = { };

	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
	ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_MINIMAL, ZYAN_TRUE);

	return decoder;
}

std::uint8_t decode_instruction(ZydisDecoder* decoder, ZydisDecodedInstruction* decoded_instruction, std::uint8_t* instruction)
{
	ZyanStatus status = ZydisDecoderDecodeInstruction(decoder, nullptr, instruction, ZYDIS_MAX_INSTRUCTION_LENGTH, decoded_instruction);

	return ZYAN_SUCCESS(status) == 1;
}

std::uint8_t get_instruction_length(const ZydisDecodedInstruction& decoded_instruction)
{
	return decoded_instruction.length;
}

std::vector<std::uint8_t> do_push(std::uint64_t value)
{
	std::vector<std::uint8_t> bytes = {
		0x68, 0x21, 0x43, 0x65, 0x87, // push   0xffffffff87654321
		0xC7, 0x44, 0x24, 0x04, 0x78, 0x56, 0x34, 0x12, // mov    DWORD PTR [rsp+0x4],0x12345678
	};

	parted_address_t parted_subroutine_to_jmp_to = { .value = value };

	*reinterpret_cast<std::uint32_t*>(&bytes[1]) = parted_subroutine_to_jmp_to.u.low_part;
	*reinterpret_cast<std::uint32_t*>(&bytes[9]) = parted_subroutine_to_jmp_to.u.high_part;

	return bytes;
}

std::vector<std::uint8_t> do_jmp(std::uint64_t address_to_jmp)
{
	std::vector<std::uint8_t> bytes = do_push(address_to_jmp);

	bytes.insert(bytes.end(), 0xC3); // ret

	return bytes;
}

std::vector<std::uint8_t> do_relative_jmp(std::int32_t offset)
{
	std::vector<std::uint8_t> bytes = { 0xE9, 0x00, 0x00, 0x00, 0x00 };

	*reinterpret_cast<std::int32_t*>(&bytes[1]) = offset;

	return bytes;
}

std::vector<std::uint8_t> do_call(std::uint64_t runtime_address, std::uint64_t address_to_call)
{
	std::vector<std::uint8_t> bytes = do_jmp(address_to_call);

	// insert return address
	std::vector<std::uint8_t> return_address_push = do_push(0); // placeholder to see the size of push

	return_address_push = do_push(runtime_address + bytes.size() + return_address_push.size());

	bytes.insert(bytes.begin(), return_address_push.begin(), return_address_push.end());

	return bytes;
}

template <class t>
std::uint64_t resolve_rip_relative_instruction(std::uint8_t* instruction, std::uint64_t offset, std::uint64_t instruction_length, std::uint64_t instruction_runtime_address)
{
	t relative = *reinterpret_cast<t*>(&instruction[offset]);

	return instruction_runtime_address + instruction_length + relative;
}

std::uint64_t resolve_rip_relative_instruction_unknown_rel_size(std::uint8_t* instruction, std::uint64_t offset, std::uint64_t instruction_length, std::uint64_t instruction_runtime_address)
{
	std::int32_t relative = 0;
	std::uint64_t opcode_size = instruction_length - offset;

	std::uint64_t size = min(sizeof(relative), opcode_size);

	memcpy(&relative, &instruction[offset], size);

	return instruction_runtime_address + instruction_length + relative;
}

// all of below are rel32 when applicable
constexpr std::array<ZydisMnemonic, 22> jcc_instructions = {
	ZYDIS_MNEMONIC_JB,
	ZYDIS_MNEMONIC_JBE,
	ZYDIS_MNEMONIC_JCXZ,
	ZYDIS_MNEMONIC_JECXZ,
	ZYDIS_MNEMONIC_JKNZD,
	ZYDIS_MNEMONIC_JKZD,
	ZYDIS_MNEMONIC_JL,
	ZYDIS_MNEMONIC_JLE,
	ZYDIS_MNEMONIC_JMP,
	ZYDIS_MNEMONIC_JNB,
	ZYDIS_MNEMONIC_JNBE,
	ZYDIS_MNEMONIC_JNL,
	ZYDIS_MNEMONIC_JNLE,
	ZYDIS_MNEMONIC_JNO,
	ZYDIS_MNEMONIC_JNP,
	ZYDIS_MNEMONIC_JNS,
	ZYDIS_MNEMONIC_JNZ,
	ZYDIS_MNEMONIC_JO,
	ZYDIS_MNEMONIC_JP,
	ZYDIS_MNEMONIC_JRCXZ,
	ZYDIS_MNEMONIC_JS,
	ZYDIS_MNEMONIC_JZ
};

std::uint8_t is_jcc_instruction(const ZydisDecodedInstruction& decoded_instruction)
{
	return std::ranges::find(jcc_instructions, decoded_instruction.mnemonic) != jcc_instructions.end();
}

std::vector<std::uint8_t> load_instruction_bytes(const ZydisDecodedInstruction& decoded_instruction, std::uint8_t* instruction, std::uint64_t instruction_runtime_address, std::uint64_t original_instruction_length, std::uint64_t routine_runtime_start, std::uint64_t minimum_routine_runtime_end, std::vector<std::pair<std::uint64_t, std::uint64_t>>& jcc_jumps)
{
	std::vector<std::uint8_t> instruction_bytes = { instruction, instruction + original_instruction_length };

	std::uint8_t is_relative = (decoded_instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE) != 0;

	if (is_relative == 1)
	{
		if (is_jcc_instruction(decoded_instruction) == 1)
		{
			std::uint64_t absolute_location = resolve_rip_relative_instruction_unknown_rel_size(instruction, 2, original_instruction_length, instruction_runtime_address);

			if (routine_runtime_start <= absolute_location && minimum_routine_runtime_end < absolute_location)
			{
				std::uint64_t instruction_aligned_bytes_offset = instruction_runtime_address - routine_runtime_start;

				jcc_jumps.push_back({ instruction_aligned_bytes_offset, absolute_location });

				// needs rel32, 2 for opcode and 4 for the rel32 address
				instruction_bytes.resize(6);

				// opcode, 0
				memcpy(instruction_bytes.data(), &instruction[0], 2);
			}
		}
		else
		{
			std::uint64_t absolute_location = resolve_rip_relative_instruction<std::int32_t>(instruction, 1, original_instruction_length, instruction_runtime_address);

			if (routine_runtime_start <= absolute_location && minimum_routine_runtime_end < absolute_location)
			{
				if (decoded_instruction.mnemonic == ZYDIS_MNEMONIC_JMP && instruction[0] == 0xE9)
				{
					instruction_bytes = do_jmp(absolute_location);
				}
				else if (decoded_instruction.mnemonic == ZYDIS_MNEMONIC_JMP && instruction[0] == 0xEB)
				{
					instruction_bytes = do_jmp(absolute_location);
				}
				else if (decoded_instruction.mnemonic == ZYDIS_MNEMONIC_CALL && instruction[0] == 0xE8)
				{
					instruction_bytes = do_call(instruction_runtime_address, absolute_location);
				}
			}
		}
	}

	return instruction_bytes;
}

void make_jcc_patches(std::vector<std::uint8_t>& aligned_bytes, const std::uint8_t* initial_instruction, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& jcc_jumps, const std::uint64_t routine_runtime_address)
{
	if (jcc_jumps.empty() == 1)
	{
		return;
	}

	std::uint64_t bytes_added = 0;

	for (auto& [instruction_aligned_bytes_offset, jmp_address] : jcc_jumps)
	{
		std::vector<std::uint8_t> bytes = do_jmp(jmp_address);

		aligned_bytes.insert(aligned_bytes.begin(), bytes.begin(), bytes.end());

		bytes_added += bytes.size();

		constexpr std::int64_t jcc_rel32_instruction_length = 6;

		std::int32_t rip_change = static_cast<std::int32_t>(-jcc_rel32_instruction_length - bytes_added - instruction_aligned_bytes_offset);

		*reinterpret_cast<std::int32_t*>(&aligned_bytes[instruction_aligned_bytes_offset + bytes_added + 2]) = rip_change;
	}

	// calculate instruction size
	std::vector<std::uint8_t> stub_pass_bytes = do_relative_jmp(0);

	std::int32_t relative_jmp_offset = static_cast<std::int32_t>(bytes_added);

	stub_pass_bytes = do_relative_jmp(relative_jmp_offset);

	aligned_bytes.insert(aligned_bytes.begin(), stub_pass_bytes.begin(), stub_pass_bytes.end());
}

std::pair<std::vector<std::uint8_t>, std::uint64_t> get_routine_aligned_bytes(std::uint8_t* routine, const std::uint64_t minimum_size, const std::uint64_t routine_runtime_address)
{
	std::uint64_t minimum_routine_runtime_end = routine_runtime_address + minimum_size;

	std::vector<std::pair<std::uint64_t, std::uint64_t>> jcc_jumps = { };

	ZydisDecoder decoder = make_zydis_decoder();

	std::vector<std::uint8_t> aligned_bytes = { };

	std::uint8_t* current_instruction = routine;

	while (aligned_bytes.size() < minimum_size)
	{
		ZydisDecodedInstruction decoded_instruction = { };

		std::uint8_t decode_status = decode_instruction(&decoder, &decoded_instruction, current_instruction);

		if (decode_status == 0)
		{
			break;
		}

		std::uint8_t original_instruction_length = get_instruction_length(decoded_instruction);
		
		std::uint64_t instruction_runtime_address = routine_runtime_address + aligned_bytes.size();

		std::vector<std::uint8_t> instruction_bytes = load_instruction_bytes(decoded_instruction, current_instruction, instruction_runtime_address, original_instruction_length, routine_runtime_address, minimum_routine_runtime_end, jcc_jumps);

		aligned_bytes.insert(aligned_bytes.end(), instruction_bytes.begin(), instruction_bytes.end());

		current_instruction += original_instruction_length;
	}

	make_jcc_patches(aligned_bytes, routine, jcc_jumps, routine_runtime_address);

	return { aligned_bytes, current_instruction - routine };
}

#define d_inline_hook_bytes_size 14

std::pair<std::vector<std::uint8_t>, std::uint64_t> load_original_bytes_into_shadow_page(std::uint8_t* shadow_page_virtual, std::uint64_t routine_to_hook_virtual, std::uint64_t routine_to_hook_physical, std::uint64_t extra_asm_byte_count)
{
	const std::uint64_t page_offset = routine_to_hook_physical & 0xFFF;

	hypercall::read_guest_physical_memory(shadow_page_virtual, routine_to_hook_physical - page_offset, 0x1000);

	return get_routine_aligned_bytes(shadow_page_virtual + page_offset, d_inline_hook_bytes_size + extra_asm_byte_count, routine_to_hook_virtual);
}

void set_up_inline_hook(std::uint8_t* shadow_page_virtual, std::uint64_t routine_to_hook_physical, std::uint64_t detour_address, std::vector<std::uint8_t> extra_assembled_bytes)
{
	std::array<std::uint8_t, d_inline_hook_bytes_size> jmp_to_detour_bytes = {
		0x68, 0x21, 0x43, 0x65, 0x87, // push   0xffffffff87654321
		0xC7, 0x44, 0x24, 0x04, 0x78, 0x56, 0x34, 0x12, // mov    DWORD PTR [rsp+0x4],0x12345678
		0xC3 // ret
	};

	parted_address_t parted_subroutine_to_jmp_to = { .value = detour_address };

	*reinterpret_cast<std::uint32_t*>(&jmp_to_detour_bytes[1]) = parted_subroutine_to_jmp_to.u.low_part;
	*reinterpret_cast<std::uint32_t*>(&jmp_to_detour_bytes[9]) = parted_subroutine_to_jmp_to.u.high_part;

	const std::uint64_t page_offset = routine_to_hook_physical & 0xFFF;

	std::vector<std::uint8_t> inline_hook_bytes = extra_assembled_bytes;

	inline_hook_bytes.insert(inline_hook_bytes.end(), jmp_to_detour_bytes.begin(), jmp_to_detour_bytes.end());

	memcpy(shadow_page_virtual + page_offset, inline_hook_bytes.data(), inline_hook_bytes.size());
}

std::uint8_t set_up_hook_handler(std::uint64_t routine_to_hook_virtual, std::uint64_t shadow_page_physical, std::uint16_t& detour_holder_shadow_offset, const std::pair<std::vector<std::uint8_t>, std::uint64_t>& original_bytes)
{
	std::array<std::uint8_t, 14> return_to_original_bytes = {
		0x68, 0x21, 0x43, 0x65, 0x87, // push   0xffffffff87654321
		0xC7, 0x44, 0x24, 0x04, 0x78, 0x56, 0x34, 0x12, // mov    DWORD PTR [rsp+0x4],0x12345678
		0xC3 // ret
	};

	parted_address_t parted_subroutine_to_jmp_to = { .value = routine_to_hook_virtual + original_bytes.second };

	*reinterpret_cast<std::uint32_t*>(&return_to_original_bytes[1]) = parted_subroutine_to_jmp_to.u.low_part;
	*reinterpret_cast<std::uint32_t*>(&return_to_original_bytes[9]) = parted_subroutine_to_jmp_to.u.high_part;

	std::vector<std::uint8_t> hook_handler_bytes = original_bytes.first;

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

	std::pair<std::vector<std::uint8_t>, std::uint64_t> original_bytes = load_original_bytes_into_shadow_page(static_cast<std::uint8_t*>(shadow_page_virtual), routine_to_hook_virtual, routine_to_hook_physical, extra_assembled_bytes.size());

	if (original_bytes.first.empty() == true)
	{
		return 0;
	}

	std::uint16_t detour_holder_shadow_offset = 0;

	std::uint8_t status = set_up_hook_handler(routine_to_hook_virtual, shadow_page_physical, detour_holder_shadow_offset, original_bytes);

	if (status == 0)
	{
		return 0;
	}

	std::uint64_t detour_address = kernel_detour_holder_base + detour_holder_shadow_offset;

	set_up_inline_hook(static_cast<std::uint8_t*>(shadow_page_virtual), routine_to_hook_physical, detour_address, extra_assembled_bytes);

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
	if (kernel_hook_list.contains(hooked_routine_virtual) == false)
	{
		std::println("unable to find kernel hook");

		return 0;
	}

	kernel_hook_info_t hook_info = kernel_hook_list[hooked_routine_virtual];

	if (hypercall::remove_slat_code_hook(hook_info.original_page_physical_address) == 0)
	{
		std::println("unable to remove slat counterpart of kernel hook");

		return 0;
	}

	kernel_hook_list.erase(hooked_routine_virtual);

	void* detour_holder_allocation = kernel_detour_holder::get_allocation_from_offset(hook_info.detour_holder_shadow_offset);

	kernel_detour_holder::free_memory(detour_holder_allocation);

	if (sys::user::free_memory(hook_info.get_mapped_shadow_page()) == 0)
	{
		std::println("unable to deallocate mapped shadow page");

		return 0;
	}

	return 1;
}
