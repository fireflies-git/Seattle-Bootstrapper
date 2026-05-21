#include "StdAfx.h"
#include "Guard.h"
#include "HttpClient.h"
#include "TicketExtractor.h"
#include "AntiDebug.h"
#include "ModuleWatcher.h"
#include "IntegrityChecker.h"
#include "Heartbeat.h"
#include "../Bootstrapper/HardwareFingerprint.h"

#include <shlobj.h>
#include <sstream>
#include <fstream>

#pragma comment(lib, "shell32.lib")

namespace
{
    bool ConsentFileExists()
    {
        wchar_t* path = nullptr;
        if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
            return false;
        std::wstring p = path ? path : L"";
        ::CoTaskMemFree(path);
        if (p.empty()) return false;
        p += L"\\Seattle\\consent.json";
        DWORD attrs = ::GetFileAttributesW(p.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES;
    }

    std::string Utf16ToUtf8(const std::wstring& w)
    {
        if (w.empty()) return std::string();
        int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
            nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
            out.data(), n, nullptr, nullptr);
        return out;
    }

    std::string EscapeJson(const std::string& s)
    {
        std::string out;
        for (char c : s)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;
            }
        }
        return out;
    }
}

DWORD WINAPI Guard::Run(LPVOID /*param*/)
{
    GuardState* state = new GuardState();

    // Bail if the user never consented at the bootstrapper layer.
    if (!ConsentFileExists())
    {
        delete state;
        return 0;
    }

    auto args = TicketExtractor::Parse();
    if (!args.ok)
    {
        delete state;
        return 0;
    }

    std::wstring basePath;
    if (!TicketExtractor::SplitHostPath(args.authUrl, state->serverHost,
            state->serverPort, state->useTls, basePath))
    {
        delete state;
        return 0;
    }

    state->ticketUtf16 = args.ticket;
    state->ticketUtf8 = Utf16ToUtf8(args.ticket);

    // Hardware hash — compute fresh. The bootstrapper has already cached a
    // copy locally; we recompute here so a hardware change between
    // bootstrap and game launch still produces an accurate signal.
    try
    {
        auto comps = HardwareFingerprint::Collect();
        state->hardwareHash = HardwareFingerprint::ComputeHash(comps);
    }
    catch (...) { /* leave empty — heartbeat still works */ }

    // POST the hardware hash up-front with the ticket-auth endpoint.
    if (!state->hardwareHash.empty())
    {
        std::ostringstream body;
        body << "{\"ticket\":\""       << EscapeJson(state->ticketUtf8) << "\","
             << "\"hardwareHash\":\""  << EscapeJson(state->hardwareHash) << "\"}";
        (void)HttpClient::PostJson(state->serverHost, state->serverPort, state->useTls,
            L"/apisite/auth/fingerprint/hw-ticket", body.str());
    }

    // Baselines for the watchers.
    ModuleWatcher::Snapshot(*state);
    IntegrityChecker::Snapshot(*state);

    // Spawn watchers. Detach handles — these threads only exit on
    // shouldExit / process termination.
    HANDLE hAntiDebug   = ::CreateThread(nullptr, 0, &AntiDebug::Run,        state, 0, nullptr);
    HANDLE hModWatcher  = ::CreateThread(nullptr, 0, &ModuleWatcher::Run,    state, 0, nullptr);
    HANDLE hIntegrity   = ::CreateThread(nullptr, 0, &IntegrityChecker::Run, state, 0, nullptr);

    if (hAntiDebug)   ::CloseHandle(hAntiDebug);
    if (hModWatcher)  ::CloseHandle(hModWatcher);
    if (hIntegrity)   ::CloseHandle(hIntegrity);

    // Heartbeat runs on this same thread.
    Heartbeat::Run(state);

    delete state;
    return 0;
}
