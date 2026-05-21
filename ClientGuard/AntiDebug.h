#pragma once

#include "Guard.h"

namespace AntiDebug
{
    // Long-running thread. Sets state.debuggerDetected when any of the
    // standard checks trip. Never aborts the process.
    DWORD WINAPI Run(LPVOID state);
}
