#include <iostream>
#include <thread>
#include <string>
#include <print>
#include <Windows.h>

#include "commands/commands.h"
#include "hook/hook.h"
#include "system/system.h"
#include "symbols/symbol_manager.h"

std::int32_t main()
{
    symbols::initialize();

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
    symbols::shutdown();

    return 0;
}
