#include "commands.h"
#include <CLI/CLI.hpp>
#include <print>

#include "../hypercall/hypercall.h"

template <class t>
t get_command_option(CLI::App* app, std::string option_name)
{
	return app->get_option(option_name)->as<t>();
}

CLI::App* init_rgpm(CLI::App& app)
{
	CLI::App* rgpm = app.add_subcommand("rgpm", "reads memory from a given guest physical address")->ignore_case();

	rgpm->add_option("physical_address")->required();
	rgpm->add_option("size")->check(CLI::Range(0, 8))->required();

	return rgpm;
}

void process_rgpm(CLI::App* rgpm)
{
	std::uint64_t guest_physical_address = get_command_option<std::uint64_t>(rgpm, "physical_address");
	std::uint64_t size = get_command_option<std::uint64_t>(rgpm, "size");
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

CLI::App* init_wgpm(CLI::App& app)
{
	CLI::App* wgpm = app.add_subcommand("wgpm", "writes memory to a given guest physical address")->ignore_case();

	wgpm->add_option("physical_address")->required();
	wgpm->add_option("value")->required();
	wgpm->add_option("size")->check(CLI::Range(0, 8))->required();

	return wgpm;
}

void process_wgpm(CLI::App* wgpm)
{
	std::uint64_t guest_physical_address = get_command_option<std::uint64_t>(wgpm, "physical_address");
	std::uint64_t size = get_command_option<std::uint64_t>(wgpm, "size");

	std::uint64_t value = get_command_option<std::uint64_t>(wgpm, "value");

	std::uint64_t bytes_read = hypercall::write_guest_physical_memory(&value, guest_physical_address, size);

	if (bytes_read == size)
	{
		std::println("success in write at given address");
	}
	else
	{
		std::println("failed to write at given address");
	}
}

CLI::App* init_cgpm(CLI::App& app)
{
	CLI::App* cgpm = app.add_subcommand("cgpm", "copies memory from a given source to a destination (guest physical addresses)")->ignore_case();

	cgpm->add_option("destination_physical_address")->required();
	cgpm->add_option("source_physical_address")->required();
	cgpm->add_option("size")->required();

	return cgpm;
}

void process_cgpm(CLI::App* cgpm)
{
	std::uint64_t guest_destination_physical_address = get_command_option<std::uint64_t>(cgpm, "destination_physical_address");
	std::uint64_t guest_source_physical_address = get_command_option<std::uint64_t>(cgpm, "source_physical_address");
	std::uint64_t size = get_command_option<std::uint64_t>(cgpm, "size");

	std::vector<std::uint8_t> buffer(size);

	std::uint64_t bytes_read = hypercall::read_guest_physical_memory(buffer.data(), guest_source_physical_address, size);
	std::uint64_t bytes_written = hypercall::write_guest_physical_memory(buffer.data(), guest_destination_physical_address, size);

	if ((bytes_read == size) && (bytes_written == size))
	{
		std::println("success in copy");
	}
	else
	{
		std::println("failed to copy");
	}
}

void commands::process(std::string command)
{
	if (command.empty() == true)
	{
		return;
	}

	CLI::App app;
	app.require_subcommand();

	CLI::App* rgpm = init_rgpm(app);
	CLI::App* wgpm = init_wgpm(app);
	CLI::App* cgpm = init_cgpm(app);

	try
	{
		app.parse(command);

		if (*rgpm)
		{
			process_rgpm(rgpm);
		}
		else if (*wgpm)
		{
			process_wgpm(wgpm);
		}
		else if (*cgpm)
		{
			process_cgpm(cgpm);
		}
	}
	catch (const CLI::ParseError& error)
	{
		app.exit(error);
	}
}

