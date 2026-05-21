#include "StdAfx.h"
#include "Heartbeat.h"
#include "HttpClient.h"

#include <sstream>

namespace
{
    std::string EscapeJson(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 4);
        for (char c : s)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if ((unsigned char)c < 0x20)
                    {
                        char buf[8];
                        sprintf_s(buf, "\\u%04x", (unsigned char)c);
                        out += buf;
                    }
                    else out += c;
            }
        }
        return out;
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
}

DWORD WINAPI Heartbeat::Run(LPVOID statePtr)
{
    GuardState* state = static_cast<GuardState*>(statePtr);

    while (!state->shouldExit.load(std::memory_order_relaxed))
    {
        // Drain new modules.
        std::vector<std::wstring> drained;
        {
            std::lock_guard<std::mutex> g(state->moduleMutex);
            drained.swap(state->pendingNewModules);
        }

        std::ostringstream body;
        body << "{\"ticket\":\"" << EscapeJson(state->ticketUtf8) << "\","
             << "\"debuggerDetected\":" << (state->debuggerDetected.load() ? "true" : "false") << ","
             << "\"integrityDrift\":"  << (state->integrityDrift.load() ? "true" : "false") << ","
             << "\"newModules\":[";
        bool first = true;
        for (const auto& w : drained)
        {
            if (!first) body << ",";
            body << "\"" << EscapeJson(Utf16ToUtf8(w)) << "\"";
            first = false;
        }
        body << "]}";

        auto resp = HttpClient::PostJson(state->serverHost, state->serverPort, state->useTls,
            L"/apisite/auth/fingerprint/heartbeat", body.str());

        // Best-effort. Drop the response — failures don't roll back the drain
        // intentionally; we'd rather miss a notification than spam on every
        // tick. The next round will report any new modules that appear.
        (void)resp;

        // 45-second cycle. Break early on shutdown.
        for (int i = 0; i < 45; ++i)
        {
            if (state->shouldExit.load(std::memory_order_relaxed)) return 0;
            ::Sleep(1000);
        }
    }

    return 0;
}
