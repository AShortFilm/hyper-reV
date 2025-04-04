#include "commands.h"
#include <CLI/CLI.hpp>
#include <print>

#include "../hypercall/hypercall.h"
#include "../system/system.h"

#define d_invoke_command_processor(command) process_##command(##command)
#define d_initial_process_command(command) if (*##command) d_invoke_command_processor(command)
#define d_process_command(command) else if (*##command) d_invoke_command_processor(command)

template <class t>
t get_command_option(CLI::App* app, std::string option_name)
{
	return app->get_option(option_name)->as<t>();
}

CLI::Option* add_command_option(CLI::App* app, std::string option_name)
{
	return app->add_option(option_name);
}

CLI::Option* add_transformed_command_option(CLI::App* app, std::string option_name, CLI::Transformer& transformer)
{
	CLI::Option* option = add_command_option(app, option_name);

	return option->transform(transformer);
}

CLI::App* init_rgpm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* rgpm = app.add_subcommand("rgpm", "reads memory from a given guest physical address")->ignore_case();

	add_transformed_command_option(rgpm, "physical_address", aliases_transformer)->required();
	add_command_option(rgpm, "size")->check(CLI::Range(0, 8))->required();

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
		std::println("failed to read");
	}
}

CLI::App* init_wgpm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* wgpm = app.add_subcommand("wgpm", "writes memory to a given guest physical address")->ignore_case();

	add_transformed_command_option(wgpm, "physical_address", aliases_transformer)->required();
	add_command_option(wgpm, "value")->required();
	add_command_option(wgpm, "size")->check(CLI::Range(0, 8))->required();

	return wgpm;
}

void process_wgpm(CLI::App* wgpm)
{
	std::uint64_t guest_physical_address = get_command_option<std::uint64_t>(wgpm, "physical_address");
	std::uint64_t size = get_command_option<std::uint64_t>(wgpm, "size");

	std::uint64_t value = get_command_option<std::uint64_t>(wgpm, "value");

	std::uint64_t bytes_written = hypercall::write_guest_physical_memory(&value, guest_physical_address, size);

	if (bytes_written == size)
	{
		std::println("success in write");
	}
	else
	{
		std::println("failed to write");
	}
}

CLI::App* init_cgpm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* cgpm = app.add_subcommand("cgpm", "copies memory from a given source to a destination (guest physical addresses)")->ignore_case();

	add_transformed_command_option(cgpm, "destination_physical_address", aliases_transformer)->required();
	add_transformed_command_option(cgpm, "source_physical_address", aliases_transformer)->required();
	add_command_option(cgpm, "size")->required();

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

CLI::App* init_gvat(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* gvat = app.add_subcommand("gvat", "translates a guest virtual address to its corresponding guest physical address, with the given guest cr3 value")->ignore_case();

	add_transformed_command_option(gvat, "virtual_address", aliases_transformer)->required();
	add_transformed_command_option(gvat, "cr3", aliases_transformer)->required();

	return gvat;
}

void process_gvat(CLI::App* gvat)
{
	std::uint64_t virtual_address = get_command_option<std::uint64_t>(gvat, "virtual_address");
	std::uint64_t cr3 = get_command_option<std::uint64_t>(gvat, "cr3");

	std::uint64_t physical_address = hypercall::translate_guest_virtual_address(virtual_address, cr3);

	std::println("physical address: 0x{:x}", physical_address);
}

CLI::App* init_rgvm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* rgvm = app.add_subcommand("rgvm", "reads memory from a given guest virtual address (when given the corresponding guest cr3 value)")->ignore_case();

	add_transformed_command_option(rgvm, "virtual_address", aliases_transformer)->required();
	add_transformed_command_option(rgvm, "cr3", aliases_transformer)->required();
	add_command_option(rgvm, "size")->check(CLI::Range(0, 8))->required();

	return rgvm;
}

void process_rgvm(CLI::App* rgvm)
{
	std::uint64_t guest_virtual_address = get_command_option<std::uint64_t>(rgvm, "virtual_address");
	std::uint64_t cr3 = get_command_option<std::uint64_t>(rgvm, "cr3");
	std::uint64_t size = get_command_option<std::uint64_t>(rgvm, "size");

	std::uint64_t value = 0;

	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(&value, guest_virtual_address, cr3, size);

	if (bytes_read == size)
	{
		std::println("value: 0x{:x}", value);
	}
	else
	{
		std::println("failed to read");
	}
}

CLI::App* init_wgvm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* wgvm = app.add_subcommand("wgvm", "writes memory from a given guest virtual address (when given the corresponding guest cr3 value)")->ignore_case();

	add_transformed_command_option(wgvm, "virtual_address", aliases_transformer)->required();
	add_transformed_command_option(wgvm, "cr3", aliases_transformer)->required();
	add_command_option(wgvm, "value")->required();
	add_command_option(wgvm, "size")->check(CLI::Range(0, 8))->required();

	return wgvm;
}

void process_wgvm(CLI::App* wgvm)
{
	std::uint64_t guest_virtual_address = get_command_option<std::uint64_t>(wgvm, "virtual_address");
	std::uint64_t cr3 = get_command_option<std::uint64_t>(wgvm, "cr3");
	std::uint64_t size = get_command_option<std::uint64_t>(wgvm, "size");
	std::uint64_t value = get_command_option<std::uint64_t>(wgvm, "value");

	std::uint64_t bytes_written = hypercall::write_guest_virtual_memory(&value, guest_virtual_address, cr3, size);

	if (bytes_written == size)
	{
		std::println("success in write at given address");
	}
	else
	{
		std::println("failed to write at given address");
	}
}

CLI::App* init_cgvm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* cgvm = app.add_subcommand("cgvm", "copies memory from a given source to a destination (guest virtual addresses) (when given the corresponding guest cr3 values)")->ignore_case();

	add_transformed_command_option(cgvm, "destination_virtual_address", aliases_transformer)->required();
	add_transformed_command_option(cgvm, "destination_cr3", aliases_transformer)->required();
	add_transformed_command_option(cgvm, "source_virtual_address", aliases_transformer)->required();
	add_transformed_command_option(cgvm, "source_cr3", aliases_transformer)->required();
	add_command_option(cgvm, "size")->required();

	return cgvm;
}

void process_cgvm(CLI::App* wgvm)
{
	std::uint64_t guest_destination_virtual_address = get_command_option<std::uint64_t>(wgvm, "destination_virtual_address");
	std::uint64_t guest_destination_cr3 = get_command_option<std::uint64_t>(wgvm, "destination_cr3");

	std::uint64_t guest_source_virtual_address = get_command_option<std::uint64_t>(wgvm, "source_virtual_address");
	std::uint64_t guest_source_cr3 = get_command_option<std::uint64_t>(wgvm, "source_cr3");

	std::uint64_t size = get_command_option<std::uint64_t>(wgvm, "size");

	std::vector<std::uint8_t> buffer(size);

	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(buffer.data(), guest_source_virtual_address, guest_source_cr3, size);
	std::uint64_t bytes_written = hypercall::write_guest_virtual_memory(buffer.data(), guest_destination_virtual_address, guest_destination_cr3, size);

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

	std::unordered_map<std::string, std::uint64_t> aliases = { { "nt", sys::ntoskrnl_base_address }, { "current_cr3", sys::current_cr3 } };

	CLI::Transformer aliases_transformer = CLI::Transformer(aliases, CLI::ignore_case);

	CLI::App* rgpm = init_rgpm(app, aliases_transformer);
	CLI::App* wgpm = init_wgpm(app, aliases_transformer);
	CLI::App* cgpm = init_cgpm(app, aliases_transformer);
	CLI::App* gvat = init_gvat(app, aliases_transformer);
	CLI::App* rgvm = init_rgvm(app, aliases_transformer);
	CLI::App* wgvm = init_wgvm(app, aliases_transformer);
	CLI::App* cgvm = init_cgvm(app, aliases_transformer);

	try
	{
		app.parse(command);

		d_initial_process_command(rgpm);
		d_process_command(wgpm);
		d_process_command(cgpm);
		d_process_command(gvat);
		d_process_command(rgvm);
		d_process_command(wgvm);
		d_process_command(cgvm);
	}
	catch (const CLI::ParseError& error)
	{
		app.exit(error);
	}
}

