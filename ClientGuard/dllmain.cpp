#include "StdAfx.h"
#include "Guard.h"

// Loader-lock rules: no WinHTTP, WMI, or LoadLibrary during DllMain. We
// spawn a worker thread and return immediately. The worker is the only
// code that touches the network / COM / heavyweight APIs.

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            ::DisableThreadLibraryCalls(hModule);
            HANDLE worker = ::CreateThread(nullptr, 0, &Guard::Run, nullptr, 0, nullptr);
            if (worker) ::CloseHandle(worker);
            break;
        }
        case DLL_PROCESS_DETACH:
            // Worker threads observe via shouldExit but the process is
            // exiting anyway — Windows will tear them down.
            break;
    }
    return TRUE;
}
