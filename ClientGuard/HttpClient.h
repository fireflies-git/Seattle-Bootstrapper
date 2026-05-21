#pragma once

#include <string>
#include <windows.h>

namespace HttpClient
{
    struct Response
    {
        bool ok = false;
        int status = 0;
        std::string body;
    };

    // POSTs a JSON body. host/port/path are in UTF-16. Returns ok=false on
    // any transport error. Never throws.
    Response PostJson(const std::wstring& host, int port, bool useTls,
                      const std::wstring& path, const std::string& jsonBody);
}
