#include "StdAfx.h"

// Same semantics as the Bootstrapper's helper. Pulled in here so the reused
// hashing code doesn't need to link against Bootstrapper.lib.
void throwLastError(BOOL result, const std::string& message)
{
    if (!result)
        throw std::runtime_error(message);
}
