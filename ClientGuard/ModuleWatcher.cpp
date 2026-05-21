#include "StdAfx.h"
#include "ModuleWatcher.h"

#include <tlhelp32.h>
#include <cwctype>

namespace
{
    std::wstring NormalizeName(const wchar_t* in)
    {
        std::wstring out(in ? in : L"");
        for (auto& c : out) c = (wchar_t)std::towlower((wint_t)c);
        return out;
    }

    std::vector<std::wstring> Enumerate()
    {
        std::vector<std::wstring> out;
        HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ::GetCurrentProcessId());
        if (snap == INVALID_HANDLE_VALUE) return out;

        MODULEENTRY32W me = { 0 };
        me.dwSize = sizeof(me);
        if (::Module32FirstW(snap, &me))
        {
            do
            {
                out.push_back(NormalizeName(me.szModule));
            } while (::Module32NextW(snap, &me));
        }
        ::CloseHandle(snap);
        return out;
    }
}

void ModuleWatcher::Snapshot(GuardState& state)
{
    auto initial = Enumerate();
    std::lock_guard<std::mutex> g(state.moduleMutex);
    state.knownModules.clear();
    for (auto& n : initial) state.knownModules.insert(n);
}

DWORD WINAPI ModuleWatcher::Run(LPVOID statePtr)
{
    GuardState* state = static_cast<GuardState*>(statePtr);

    while (!state->shouldExit.load(std::memory_order_relaxed))
    {
        ::Sleep(30000);
        if (state->shouldExit.load(std::memory_order_relaxed)) break;

        auto current = Enumerate();
        std::vector<std::wstring> newly;

        {
            std::lock_guard<std::mutex> g(state->moduleMutex);
            for (auto& name : current)
            {
                if (state->knownModules.find(name) == state->knownModules.end())
                {
                    state->knownModules.insert(name);
                    newly.push_back(name);
                }
            }
            for (auto& n : newly) state->pendingNewModules.push_back(n);
        }
    }

    return 0;
}
