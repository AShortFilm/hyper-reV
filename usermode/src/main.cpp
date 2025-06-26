#include <iostream>
#include <thread>
#include <string>
#include <print>
#include <Windows.h>

#include "commands/commands.h"
#include "hook/hook.h"
#include "hypercall/hypercall.h"
#include "system/system.h"

extern "C" std::uint64_t launch_raw_hypercall(std::uint64_t rcx, std::uint64_t rdx, std::uint64_t r8, std::uint64_t r9);

std::int32_t main()
{
    if (sys::set_up() == 0)
    {
        std::system("pause");

        return 1;
    }

	while (true)
	{
		std::print("> ");

		std::string command = { };
		std::getline(std::cin, command);

		if (command == "exit")
		{
			break;
		}
			
		commands::process(command);

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	sys::clean_up();

	return 0;
}
