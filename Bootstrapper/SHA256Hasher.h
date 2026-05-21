#pragma once
#include <string>

// SHA-256 wrapper. Uses BCrypt (Vista+) so the bootstrapper's XP-era
// _WIN32_WINNT in targetver.h doesn't block compilation.
class SHA256Hasher
{
	std::string result;
	void* hAlg;    // BCRYPT_ALG_HANDLE
	void* hHash;   // BCRYPT_HASH_HANDLE
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
