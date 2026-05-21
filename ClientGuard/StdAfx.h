#pragma once

// Minimal stand-in for the Bootstrapper's StdAfx so the reused .cpp files
// (HardwareFingerprint, SHA256Hasher) compile inside the ClientGuard DLL
// build without pulling in ATL / the full Bootstrapper precompiled header.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <stdexcept>

// throwLastError is `extern`-declared in MD5Hasher.cpp / SHA256Hasher.cpp.
// Defined in StubSupport.cpp in this DLL project.
void throwLastError(BOOL result, const std::string& message);
