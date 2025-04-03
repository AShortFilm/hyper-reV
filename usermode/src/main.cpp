#include <iostream>
#include <thread>
#include <string>
#include <print>

#include "commands/commands.h"

std::int32_t main()
{
	while (true)
	{
		std::print("> ");

		std::string command = { };
		std::getline(std::cin, command);

		commands::process(command);

		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}

	return 0;
}
