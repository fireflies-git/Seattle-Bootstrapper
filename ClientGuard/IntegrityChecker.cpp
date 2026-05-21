#include "StdAfx.h"
#include "IntegrityChecker.h"
#include "../Bootstrapper/SHA256Hasher.h"

#include <vector>

namespace
{
    bool LocateTextSection(unsigned char*& outBase, size_t& outSize)
    {
        HMODULE base = ::GetModuleHandleW(nullptr);
        if (!base) return false;

        auto dos = (PIMAGE_DOS_HEADER)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto nt = (PIMAGE_NT_HEADERS)((unsigned char*)base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        auto section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if (memcmp(section->Name, ".text", 5) == 0)
            {
                outBase = (unsigned char*)base + section->VirtualAddress;
                outSize = section->Misc.VirtualSize;
                return true;
            }
        }
        return false;
    }

    bool HashText(std::vector<unsigned char>& out)
    {
        unsigned char* base = nullptr;
        size_t size = 0;
        if (!LocateTextSection(base, size)) return false;

        SHA256Hasher h;
        // Read in chunks to avoid touching anything we shouldn't.
        const size_t chunk = 64 * 1024;
        for (size_t off = 0; off < size; off += chunk)
        {
            size_t n = (size - off) > chunk ? chunk : (size - off);
            // Use SEH so a missing/swapped page can't crash the worker.
            __try
            {
                h.addData((const char*)(base + off), n);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        const std::string& hex = h.toString();
        out.assign(hex.begin(), hex.end());
        return true;
    }
}

bool IntegrityChecker::Snapshot(GuardState& state)
{
    return HashText(state.textBaselineHash);
}

DWORD WINAPI IntegrityChecker::Run(LPVOID statePtr)
{
    GuardState* state = static_cast<GuardState*>(statePtr);

    while (!state->shouldExit.load(std::memory_order_relaxed))
    {
        ::Sleep(60000);
        if (state->shouldExit.load(std::memory_order_relaxed)) break;

        std::vector<unsigned char> current;
        if (!HashText(current)) continue;

        if (current != state->textBaselineHash)
            state->integrityDrift.store(true, std::memory_order_relaxed);
    }

    return 0;
}
