#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace hook
{
	std::uint8_t set_up();

	std::uint8_t add_kernel_hook(std::uint64_t routine_to_hook_virtual, std::vector<std::uint8_t> extra_assembled_bytes);
	std::uint8_t remove_kernel_hook(std::uint64_t hooked_routine_virtual);

	struct kernel_hook_info_t
	{
		std::uint64_t original_page_physical_address;

		struct
		{
			std::uint64_t mapped_shadow_page : 48;
			std::uint16_t detour_holder_shadow_offset : 16;
		};

		void* get_mapped_shadow_page()
		{
			return reinterpret_cast<void*>(this->mapped_shadow_page);
		}

		void set_mapped_shadow_page(void* pointer)
		{
			this->mapped_shadow_page = reinterpret_cast<std::uint64_t>(pointer);
		}
	};

	inline std::unordered_map<std::uint64_t, kernel_hook_info_t> kernel_hook_list = { };

	inline std::uint64_t kernel_detour_holder_base = 0;
	inline std::uint64_t kernel_detour_holder_physical_page = 0;
	inline std::uint8_t* kernel_detour_holder_shadow_page_mapped = nullptr;
}
