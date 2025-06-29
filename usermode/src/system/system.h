#pragma once
#include <unordered_map>
#include <optional>
#include <string>
#include "system_def.h"

namespace sys
{
	struct kernel_module_t;

	std::uint8_t set_up();
	void clean_up();

	std::uint8_t acquire_privilege();

	namespace kernel
	{
		std::vector<rtl_process_module_information_t> get_loaded_modules();
		std::optional<rtl_process_module_information_t> get_module_information(std::string_view target_module_name);

		std::uint8_t parse_modules();

		inline std::unordered_map<std::string, kernel_module_t> modules_list = { };
	}

	namespace user
	{
		std::uint32_t query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size, std::uint32_t* returned_size);
		std::uint32_t adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only, std::uint8_t* previous_enabled_state);
		void* allocate_locked_memory(std::uint64_t size, std::uint32_t protection);
		std::uint8_t free_memory(void* address);

		std::string to_string(const std::wstring& wstring);
	}

	struct kernel_module_t
	{
		std::unordered_map<std::string, std::uint64_t> exports;
		
		std::uint64_t base_address;
		std::uint32_t size;
	};

	inline std::uint64_t current_cr3 = 0;
}
