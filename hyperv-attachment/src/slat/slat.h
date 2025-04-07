#pragma once
#include <ia32-doc/ia32.hpp>
#include "../structures/virtual_address.h"

namespace slat
{
	void set_up();

	cr3 get_cr3();

	std::uint64_t translate_guest_physical_address(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint64_t* size_left_of_slat_page = nullptr);
	std::uint64_t add_slat_code_hook(cr3 slat_cr3, virtual_address_t guest_physical_address, virtual_address_t shadow_page_guest_physical_address);
	std::uint64_t remove_slat_code_hook(cr3 slat_cr3, virtual_address_t guest_physical_address);

	std::uint8_t process_slat_violation();

	class hook_entry_t
	{
	private:
		std::uint64_t next : 48;
		std::uint64_t original_read_access : 1;
		std::uint64_t original_write_access : 1;
		std::uint64_t original_execute_access : 1;
		std::uint64_t reserved : 5;

		std::uint64_t higher_original_pfn : 4;
		std::uint64_t higher_shadow_pfn : 4;

		std::uint32_t lower_original_pfn;
		std::uint32_t lower_shadow_pfn;

	public:
		hook_entry_t* get_next() const;
		void set_next(hook_entry_t* next_entry);

		std::uint64_t get_original_pfn() const;
		std::uint64_t get_shadow_pfn() const;

		std::uint64_t get_original_read_access() const;
		std::uint64_t get_original_write_access() const;
		std::uint64_t get_original_execute_access() const;

		void set_original_pfn(std::uint64_t original_pfn);
		void set_shadow_pfn(std::uint64_t shadow_pfn);

		void set_original_read_access(std::uint64_t original_read_access_in);
		void set_original_write_access(std::uint64_t original_write_access_in);
		void set_original_execute_access(std::uint64_t original_execute_access_in);

		static hook_entry_t* find(hook_entry_t* list_head, std::uint64_t target_original_pfn, hook_entry_t** previous_entry_out = nullptr);
	};

	inline hook_entry_t* available_hook_list_head = nullptr;
	inline hook_entry_t* used_hook_list_head = nullptr;
}
