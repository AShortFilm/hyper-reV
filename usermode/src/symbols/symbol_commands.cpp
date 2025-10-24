#include "symbol_commands.h"

#include "symbol_manager.h"

#include <CLI/CLI.hpp>

#include <iomanip>
#include <print>
#include <ranges>
#include <string>

namespace symbol_commands
{
    namespace
    {
        template <typename Value>
        Value get_option(CLI::App* command, const std::string& name)
        {
            auto option = command->get_option(name);
            return option->as<Value>();
        }

        CLI::App* init_attach(CLI::App& app)
        {
            CLI::App* attach = app.add_subcommand("sap", "attach to a process for symbol parsing")->ignore_case();
            attach->add_option("pid", "process identifier to attach", true)->check(CLI::Range(1u, 0xffffffffu))->required();
            return attach;
        }

        CLI::App* init_detach(CLI::App& app)
        {
            return app.add_subcommand("sdp", "detach from the current symbol process")->ignore_case();
        }

        CLI::App* init_list_modules(CLI::App& app)
        {
            return app.add_subcommand("slm", "list modules of the attached process")->ignore_case();
        }

        CLI::App* init_resolve_symbol(CLI::App& app)
        {
            CLI::App* resolve = app.add_subcommand("srs", "resolve the address of a symbol (e.g. ntdll!NtCreateFile)")->ignore_case();
            resolve->add_option("symbol", "symbol name to resolve", true)->required();
            return resolve;
        }

        CLI::App* init_resolve_address(CLI::App& app, CLI::Transformer& aliases)
        {
            CLI::App* resolve = app.add_subcommand("sra", "resolve an address to its symbol name")->ignore_case();
            resolve->add_option("address", "address to resolve", true)->required()->transform(aliases);
            return resolve;
        }

        CLI::App* init_read_memory(CLI::App& app, CLI::Transformer& aliases)
        {
            CLI::App* read = app.add_subcommand("srm", "read memory from the attached process (virtual address)")->ignore_case();
            read->add_option("address", "virtual address to read", true)->required()->transform(aliases);
            read->add_option("size", "number of bytes to read", true)->required();
            return read;
        }

        CLI::App* g_attach = nullptr;
        CLI::App* g_detach = nullptr;
        CLI::App* g_modules = nullptr;
        CLI::App* g_symbol_address = nullptr;
        CLI::App* g_address_symbol = nullptr;
        CLI::App* g_read_memory = nullptr;

        void process_attach()
        {
            auto pid = get_option<std::uint32_t>(g_attach, "pid");
            if (symbols::attach(pid))
            {
                std::println("attached to process {}", pid);
                return;
            }

            std::println("failed to attach to process {}", pid);
        }

        void process_detach()
        {
            if (symbols::is_attached())
            {
                symbols::detach();
                std::println("detached from process");
            }
            else
            {
                std::println("no active symbol session");
            }
        }

        void process_modules()
        {
            if (!symbols::is_attached())
            {
                std::println("attach to a process first");
                return;
            }

            const auto& modules = symbols::modules();

            if (modules.empty())
            {
                std::println("no modules found");
                return;
            }

            for (const auto& module : modules)
            {
                const std::string name(module.name.begin(), module.name.end());
                const std::string path(module.path.begin(), module.path.end());

                std::println("{} - base: 0x{:X}, size: 0x{:X} - {}", name, module.base_address, module.size, path);
            }
        }

        void process_symbol_address()
        {
            if (!symbols::is_attached())
            {
                std::println("attach to a process first");
                return;
            }

            const auto symbol_name = get_option<std::string>(g_symbol_address, "symbol");
            auto address = symbols::resolve_symbol_address(symbol_name);

            if (!address.has_value())
            {
                std::println("failed to resolve symbol '{}'", symbol_name);
                return;
            }

            std::println("{} = 0x{:X}", symbol_name, address.value());
        }

        void process_address_symbol()
        {
            if (!symbols::is_attached())
            {
                std::println("attach to a process first");
                return;
            }

            const auto address = get_option<std::uint64_t>(g_address_symbol, "address");
            auto name = symbols::resolve_address_name(address);

            if (!name.has_value())
            {
                std::println("failed to resolve address 0x{:X}", address);
                return;
            }

            std::println("0x{:X} = {}", address, name.value());
        }

        void process_read_memory()
        {
            if (!symbols::is_attached())
            {
                std::println("attach to a process first");
                return;
            }

            const auto address = get_option<std::uint64_t>(g_read_memory, "address");
            const auto size = get_option<std::size_t>(g_read_memory, "size");

            auto data = symbols::read_memory(address, size);
            if (!data.has_value())
            {
                std::println("failed to read memory at 0x{:X}", address);
                return;
            }

            std::println("memory at 0x{:X}:", address);

            std::size_t index = 0;
            for (auto byte : data.value())
            {
                std::print("{:02X} ", static_cast<unsigned>(byte));
                ++index;

                if (index % 16 == 0)
                {
                    std::println("");
                }
            }

            if (index % 16 != 0)
            {
                std::println("");
            }
        }
    } // namespace

    void init_commands(CLI::App& app, CLI::Transformer& aliases_transformer)
    {
        g_attach = init_attach(app);
        g_detach = init_detach(app);
        g_modules = init_list_modules(app);
        g_symbol_address = init_resolve_symbol(app);
        g_address_symbol = init_resolve_address(app, aliases_transformer);
        g_read_memory = init_read_memory(app, aliases_transformer);

        g_attach->callback(process_attach);
        g_detach->callback(process_detach);
        g_modules->callback(process_modules);
        g_symbol_address->callback(process_symbol_address);
        g_address_symbol->callback(process_address_symbol);
        g_read_memory->callback(process_read_memory);
    }
}
