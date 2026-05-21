#pragma once
#include "wincrypt.h"
#include <string>

// Mirrors MD5Hasher but uses CALG_SHA_256. Used for the hardware-fingerprint
// composite hash sent to the alt-detection backend.
class SHA256Hasher
{
	std::string result;
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
public:
	SHA256Hasher(void);
	~SHA256Hasher(void);
	void addData(const std::string& data);
	void addData(const char* data, size_t nBytes);
	const std::string& toString()
	{
		c_str();
		return result;
	}

	const char* c_str();
};
