#include "symbol_manager.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace symbols
{
    namespace
    {
        std::mutex g_guard;
        HANDLE g_process_handle = nullptr;
        std::uint32_t g_process_id = 0;
        bool g_dbghelp_initialized = false;
        std::vector<module_info> g_modules;

        std::string narrow(const std::wstring& value)
        {
            if (value.empty())
            {
                return {};
            }

            int required = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (required <= 0)
            {
                return {};
            }

            std::string result(static_cast<std::size_t>(required), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
            return result;
        }

        bool initialize_dbghelp()
        {
            if (g_dbghelp_initialized)
            {
                return true;
            }

            DWORD options = ::SymGetOptions();
            options |= SYMOPT_UNDNAME;
            options |= SYMOPT_DEFERRED_LOADS;
            options |= SYMOPT_FAIL_CRITICAL_ERRORS;
            options |= SYMOPT_CASE_INSENSITIVE;
            ::SymSetOptions(options);

            g_dbghelp_initialized = true;
            return true;
        }

        void cleanup_dbghelp()
        {
            g_dbghelp_initialized = false;
        }

        void unload_modules_locked()
        {
            if (g_process_handle == nullptr)
            {
                g_modules.clear();
                return;
            }

            ::SymCleanup(g_process_handle);
            ::CloseHandle(g_process_handle);

            g_process_handle = nullptr;
            g_process_id = 0;
            g_modules.clear();
        }

        module_info make_module_info(const MODULEENTRY32W& entry)
        {
            module_info info = {};
            info.name = entry.szModule;
            info.path = entry.szExePath;
            info.base_address = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            info.size = entry.modBaseSize;
            return info;
        }

        void ensure_symbol_path(HANDLE process)
        {
            constexpr std::wstring_view default_path = L"srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";

            std::array<wchar_t, 4096> buffer = {};
            DWORD path_length = ::GetEnvironmentVariableW(L"_NT_SYMBOL_PATH", buffer.data(), static_cast<DWORD>(buffer.size()));

            if (path_length == 0)
            {
                ::SymSetSearchPathW(process, default_path.data());
            }
        }

        std::optional<std::vector<std::uint8_t>> read_memory_locked(std::uint64_t address, std::size_t size)
        {
            if (g_process_handle == nullptr)
            {
                return std::nullopt;
            }

            std::vector<std::uint8_t> buffer(size);
            SIZE_T bytes_read = 0;

            if (::ReadProcessMemory(g_process_handle, reinterpret_cast<LPCVOID>(address), buffer.data(), size, &bytes_read) == FALSE)
            {
                return std::nullopt;
            }

            buffer.resize(bytes_read);
            return buffer;
        }

    } // namespace

    bool initialize()
    {
        std::scoped_lock lock(g_guard);
        return initialize_dbghelp();
    }

    void shutdown()
    {
        std::scoped_lock lock(g_guard);
        unload_modules_locked();
        cleanup_dbghelp();
    }

    bool attach(std::uint32_t process_id)
    {
        std::scoped_lock lock(g_guard);

        initialize_dbghelp();
        unload_modules_locked();

        HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id);
        if (process == nullptr)
        {
            return false;
        }

        if (::SymInitialize(process, nullptr, FALSE) == FALSE)
        {
            ::CloseHandle(process);
            return false;
        }

        ensure_symbol_path(process);

        g_process_handle = process;
        g_process_id = process_id;

        return reload_modules();
    }

    void detach()
    {
        std::scoped_lock lock(g_guard);
        unload_modules_locked();
    }

    bool is_attached()
    {
        std::scoped_lock lock(g_guard);
        return g_process_handle != nullptr;
    }

    std::uint32_t current_process_id()
    {
        std::scoped_lock lock(g_guard);
        return g_process_id;
    }

    const std::vector<module_info>& modules()
    {
        std::scoped_lock lock(g_guard);
        return g_modules;
    }

    bool reload_modules()
    {
        std::scoped_lock lock(g_guard);

        if (g_process_handle == nullptr)
        {
            return false;
        }

        g_modules.clear();

        HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, g_process_id);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        MODULEENTRY32W entry = {};
        entry.dwSize = sizeof(entry);

        if (::Module32FirstW(snapshot, &entry) == FALSE)
        {
            ::CloseHandle(snapshot);
            return false;
        }

        do
        {
            module_info info = make_module_info(entry);

            ::SymLoadModuleExW(
                g_process_handle,
                nullptr,
                entry.szExePath,
                entry.szModule,
                reinterpret_cast<DWORD64>(entry.modBaseAddr),
                entry.modBaseSize,
                nullptr,
                0);

            g_modules.emplace_back(std::move(info));
        } while (::Module32NextW(snapshot, &entry));

        ::CloseHandle(snapshot);
        return true;
    }

    std::optional<std::uint64_t> resolve_symbol_address(const std::string& symbol_name)
    {
        std::scoped_lock lock(g_guard);

        if (g_process_handle == nullptr)
        {
            return std::nullopt;
        }

        std::array<std::uint8_t, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> buffer = {};
        PSYMBOL_INFO symbol_info = reinterpret_cast<PSYMBOL_INFO>(buffer.data());
        symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol_info->MaxNameLen = MAX_SYM_NAME;

        if (::SymFromName(g_process_handle, symbol_name.c_str(), symbol_info) == FALSE)
        {
            return std::nullopt;
        }

        return symbol_info->Address;
    }

    std::optional<std::string> resolve_address_name(std::uint64_t address)
    {
        std::scoped_lock lock(g_guard);

        if (g_process_handle == nullptr)
        {
            return std::nullopt;
        }

        std::array<std::uint8_t, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> buffer = {};
        PSYMBOL_INFO symbol_info = reinterpret_cast<PSYMBOL_INFO>(buffer.data());
        symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol_info->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        if (::SymFromAddr(g_process_handle, address, &displacement, symbol_info) == FALSE)
        {
            return std::nullopt;
        }

        std::ostringstream stream;
        stream << symbol_info->Name;
        if (displacement != 0)
        {
            stream << "+0x" << std::hex << displacement;
        }

        return stream.str();
    }

    std::optional<std::vector<std::uint8_t>> read_memory(std::uint64_t address, std::size_t size)
    {
        std::scoped_lock lock(g_guard);
        return read_memory_locked(address, size);
    }

} // namespace symbols
