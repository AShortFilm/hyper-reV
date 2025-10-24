#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace symbols
{
    struct module_info
    {
        std::wstring name;
        std::wstring path;
        std::uint64_t base_address;
        std::uint32_t size;
    };

    bool initialize();
    void shutdown();

    bool attach(std::uint32_t process_id);
    void detach();
    bool is_attached();
    std::uint32_t current_process_id();

    const std::vector<module_info>& modules();
    bool reload_modules();

    std::optional<std::uint64_t> resolve_symbol_address(const std::string& symbol_name);
    std::optional<std::string> resolve_address_name(std::uint64_t address);

    std::optional<std::vector<std::uint8_t>> read_memory(std::uint64_t address, std::size_t size);
}
