#include "StdAfx.h"
#include "HttpClient.h"

#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace
{
    struct WinHttpHandle
    {
        HINTERNET h = nullptr;
        ~WinHttpHandle() { if (h) ::WinHttpCloseHandle(h); }
        operator HINTERNET() const { return h; }
    };
}

HttpClient::Response HttpClient::PostJson(const std::wstring& host, int port, bool useTls,
                                          const std::wstring& path, const std::string& jsonBody)
{
    Response r;

    WinHttpHandle session;
    session.h = ::WinHttpOpen(L"SeattleClientGuard/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return r;

    WinHttpHandle connect;
    connect.h = ::WinHttpConnect(session, host.c_str(), (INTERNET_PORT)port, 0);
    if (!connect) return r;

    DWORD openFlags = useTls ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request;
    request.h = ::WinHttpOpenRequest(connect, L"POST", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openFlags);
    if (!request) return r;

    const wchar_t* headers = L"Content-Type: application/json\r\nUser-Agent: SeattleClientGuard/1.0\r\n";

    BOOL sent = ::WinHttpSendRequest(request, headers, (DWORD)-1L,
        (LPVOID)jsonBody.data(), (DWORD)jsonBody.size(), (DWORD)jsonBody.size(), 0);
    if (!sent) return r;

    if (!::WinHttpReceiveResponse(request, nullptr)) return r;

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!::WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
        return r;
    r.status = (int)status;

    std::string body;
    for (;;)
    {
        DWORD available = 0;
        if (!::WinHttpQueryDataAvailable(request, &available)) return r;
        if (available == 0) break;

        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!::WinHttpReadData(request, chunk.data(), available, &read)) return r;
        chunk.resize(read);
        body += chunk;
    }

    r.body = std::move(body);
    r.ok = (r.status >= 200 && r.status < 300);
    return r;
}
