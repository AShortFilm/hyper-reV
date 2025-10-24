#pragma once

namespace CLI { class App; class Transformer; }

namespace symbol_commands
{
    void init_commands(CLI::App& app, CLI::Transformer& aliases_transformer);
}
