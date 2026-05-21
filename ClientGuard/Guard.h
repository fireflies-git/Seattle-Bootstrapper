#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_set>

// Shared state for the protection threads. Updates from worker threads are
// drained by the heartbeat loop and posted to the server.
struct GuardState
{
    std::wstring serverHost;    // host extracted from -authenticationUrl
    int serverPort = 443;
    bool useTls = true;
    std::wstring ticketUtf16;   // raw -authenticationTicket value (JWT)
    std::string ticketUtf8;
    std::string hardwareHash;

    std::atomic<bool> debuggerDetected{false};
    std::atomic<bool> integrityDrift{false};

    std::mutex moduleMutex;
    std::unordered_set<std::wstring> knownModules; // case-insensitive lower
    std::vector<std::wstring> pendingNewModules;   // drained by heartbeat

    std::vector<unsigned char> textBaselineHash;   // sha-256 of .text on startup

    std::atomic<bool> shouldExit{false};
};

namespace Guard
{
    // Worker entry point spawned by DllMain. Returns when the process exits
    // or the state is asked to stop.
    DWORD WINAPI Run(LPVOID param);
}
