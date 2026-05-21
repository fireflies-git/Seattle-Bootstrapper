#include "StdAfx.h"
#include "AntiDebug.h"

#include <winternl.h>

namespace
{
    typedef NTSTATUS(NTAPI *PFN_NtQueryInformationProcess)(
        HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

    bool ProcessDebugPortSet()
    {
        static PFN_NtQueryInformationProcess fn = nullptr;
        if (!fn)
        {
            HMODULE m = ::GetModuleHandleW(L"ntdll.dll");
            if (!m) return false;
            fn = (PFN_NtQueryInformationProcess)::GetProcAddress(m, "NtQueryInformationProcess");
            if (!fn) return false;
        }

        HANDLE port = nullptr;
        ULONG ret = 0;
        // ProcessDebugPort = 7
        NTSTATUS s = fn(::GetCurrentProcess(), (PROCESSINFOCLASS)7, &port, sizeof(port), &ret);
        if (s != 0) return false;
        return port != nullptr;
    }

    bool PebBeingDebugged()
    {
#if defined(_M_IX86)
        // PEB->BeingDebugged is at offset 2 from PEB base; fs:[0x30] = PEB.
        unsigned char beingDebugged = 0;
        __try
        {
            __asm
            {
                mov eax, fs:[0x30]
                mov al, byte ptr [eax + 2]
                mov beingDebugged, al
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return beingDebugged != 0;
#else
        return false;
#endif
    }
}

DWORD WINAPI AntiDebug::Run(LPVOID statePtr)
{
    GuardState* state = static_cast<GuardState*>(statePtr);

    while (!state->shouldExit.load(std::memory_order_relaxed))
    {
        bool flagged = false;

        if (::IsDebuggerPresent()) flagged = true;

        BOOL remote = FALSE;
        if (!flagged && ::CheckRemoteDebuggerPresent(::GetCurrentProcess(), &remote) && remote)
            flagged = true;

        if (!flagged && ProcessDebugPortSet()) flagged = true;
        if (!flagged && PebBeingDebugged()) flagged = true;

        if (flagged)
            state->debuggerDetected.store(true, std::memory_order_relaxed);

        // Don't reset to false — once detected during this session, stays
        // flagged. Reduces false positives from transient races.
        ::Sleep(10000);
    }

    return 0;
}
