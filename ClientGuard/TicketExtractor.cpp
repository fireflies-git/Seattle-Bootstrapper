#include "StdAfx.h"
#include "TicketExtractor.h"

#include <shellapi.h>
#include <winhttp.h>
#include <cwctype>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

namespace
{
    bool EqualsIgnoreCase(const wchar_t* a, const wchar_t* b)
    {
        while (*a && *b)
        {
            if (std::towlower((wint_t)*a) != std::towlower((wint_t)*b)) return false;
            ++a; ++b;
        }
        return *a == 0 && *b == 0;
    }
}

TicketExtractor::LaunchArgs TicketExtractor::Parse()
{
    LaunchArgs out;

    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) return out;

    // Named-flag form: try first.
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (EqualsIgnoreCase(argv[i], L"-authenticationTicket"))
            out.ticket = argv[i + 1];
        if (EqualsIgnoreCase(argv[i], L"-authenticationUrl"))
            out.authUrl = argv[i + 1];
    }

    // Positional fallback for the `-play` layout the bootstrapper currently
    // uses: exe -play script authUrl ticket silent unhide
    if (out.ticket.empty() || out.authUrl.empty())
    {
        for (int i = 1; i < argc; ++i)
        {
            if (EqualsIgnoreCase(argv[i], L"-play") && i + 3 < argc)
            {
                if (out.authUrl.empty()) out.authUrl = argv[i + 2];
                if (out.ticket.empty())  out.ticket  = argv[i + 3];
                break;
            }
        }
    }

    ::LocalFree(argv);
    out.ok = !out.ticket.empty() && !out.authUrl.empty();
    return out;
}

bool TicketExtractor::SplitHostPath(const std::wstring& url, std::wstring& host, int& port,
                                    bool& useTls, std::wstring& basePath)
{
    URL_COMPONENTSW comp = { 0 };
    comp.dwStructSize = sizeof(comp);
    comp.dwSchemeLength = (DWORD)-1;
    comp.dwHostNameLength = (DWORD)-1;
    comp.dwUrlPathLength = (DWORD)-1;

    if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &comp))
        return false;

    host.assign(comp.lpszHostName, comp.dwHostNameLength);
    basePath.assign(comp.lpszUrlPath, comp.dwUrlPathLength);
    port = comp.nPort != 0 ? comp.nPort : 443;

    if (comp.nScheme == INTERNET_SCHEME_HTTPS) useTls = true;
    else if (comp.nScheme == INTERNET_SCHEME_HTTP) useTls = false;
    else useTls = true;

    return true;
}
