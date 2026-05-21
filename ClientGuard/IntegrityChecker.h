#pragma once

#include "Guard.h"

namespace IntegrityChecker
{
    // Computes a SHA-256 of the .text section of the main exe and stores it
    // in state.textBaselineHash. Returns false if the section can't be
    // located (very old / packed binaries).
    bool Snapshot(GuardState& state);

    DWORD WINAPI Run(LPVOID state);
}
