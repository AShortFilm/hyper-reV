#include "commands.h"
#include <CLI/CLI.hpp>
#include <print>

#include "../hypercall/hypercall.h"

void commands::process(std::string command)
{
	if (command.empty() == true)
	{
		return;
	}

	CLI::App app;
	app.require_subcommand();

	CLI::App* read_guest_physical_memory = app.add_subcommand("rgpm", "reads guest physical memory from given address")->ignore_case();

	read_guest_physical_memory->add_option("physical_address")->required();
	read_guest_physical_memory->add_option("size")->check(CLI::Range(0, 8))->required();

	try
	{
		app.parse(command);

		if (*read_guest_physical_memory)
		{
			std::uint64_t guest_physical_address = read_guest_physical_memory->get_option("physical_address")->as<std::uint64_t>();
			std::uint64_t size = read_guest_physical_memory->get_option("size")->as<std::uint64_t>();

			std::uint64_t value = 0;

			std::uint64_t bytes_read = hypercall::read_guest_physical_memory(&value, guest_physical_address, size);

			if (bytes_read == size)
			{
				std::println("value: 0x{:x}", value);
			}
			else
			{
				std::println("failed to read at given address");
			}
		}
	}
	catch (const CLI::ParseError& error)
	{
		app.exit(error);
	}
}

