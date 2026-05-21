#pragma once

#include "Guard.h"

namespace ModuleWatcher
{
    // Capture the initial set of loaded modules. Subsequent changes are
    // reported via state.pendingNewModules.
    void Snapshot(GuardState& state);

    DWORD WINAPI Run(LPVOID state);
}
