#pragma once

#include "Guard.h"

namespace Heartbeat
{
    // Long-running loop. Builds heartbeat JSON, posts to the server, drains
    // pendingNewModules under the mutex. Sleeps 45s between POSTs.
    DWORD WINAPI Run(LPVOID state);
}
