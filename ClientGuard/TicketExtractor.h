#pragma once

#include <string>

namespace TicketExtractor
{
    struct LaunchArgs
    {
        std::wstring ticket;
        std::wstring authUrl;
        bool ok = false;
    };

    // Reads GetCommandLineW() and returns the launch ticket + authentication
    // URL. Supports both the named-flag form ("-authenticationTicket VAL")
    // and the positional form used by the bootstrapper today
    // ("exe -play script authUrl ticket ..." — see SharedLauncher.cpp:280).
    LaunchArgs Parse();

    // Cracks an https://host[:port]/path URL into pieces. Defaults to 443.
    bool SplitHostPath(const std::wstring& url, std::wstring& host, int& port,
                       bool& useTls, std::wstring& basePath);
}
