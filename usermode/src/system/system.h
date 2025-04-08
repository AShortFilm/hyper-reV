#pragma once
#include <unordered_map>
#include <optional>
#include <string>
#include "system_def.h"

namespace sys
{
	std::uint8_t set_up();
	void clean_up();

	std::uint8_t acquire_privilege();

	namespace kernel
	{
		std::optional<rtl_process_module_information_t> get_module_information(std::string_view target_module_name);
	}

	namespace user
	{
		std::uint32_t query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size, std::uint32_t* returned_size);
		std::uint32_t adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only, std::uint8_t* previous_enabled_state);
		void* allocate_locked_memory(std::uint64_t size, std::uint32_t protection);
		std::uint8_t free_memory(void* address);
	}

	inline std::unordered_map<std::string, std::uint64_t> ntoskrnl_exports = { };

	inline std::uint64_t ntoskrnl_base_address = 0;
	inline std::uint64_t current_cr3 = 0;
}
