#pragma once

#include <string>
#include <vector>

// Collects stable hardware identifiers via WMI and combines them into a
// SHA-256 hex string. The raw identifiers never leave the process — only
// the composite hash is exposed.
//
// The local cache is encrypted at rest via DPAPI (CRYPTPROTECT_LOCAL_MACHINE)
// and lives at %LOCALAPPDATA%\Seattle\hwfp.dat. The cache is recomputed every
// launch; a hardware change legitimately produces a new hash.
class HardwareFingerprint
{
public:
	struct Components
	{
		std::string cpuId;
		std::string mbSerial;
		std::string biosSerial;
		std::string systemUuid;
		std::string diskSerial;
		std::string macAddress;
		std::string gpuDeviceId;
		std::vector<std::string> missing;
	};

	// Collects identifiers via WMI with registry fallbacks. Never throws —
	// failures simply append to `missing`.
	static Components Collect();

	// SHA-256 hex of the canonicalised concatenation of the components.
	static std::string ComputeHash(const Components& c);

	// Convenience: collect + hash + cache locally (DPAPI-encrypted).
	static std::string CollectAndHashCached();
};
