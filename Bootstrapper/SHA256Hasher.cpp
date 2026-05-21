#include "StdAfx.h"
#include "SHA256Hasher.h"
#include "format_string.h"

extern void throwLastError(BOOL result, const std::string& message);

SHA256Hasher::SHA256Hasher(void)
{
	// PROV_RSA_AES is the provider that exposes CALG_SHA_256.
	throwLastError(CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT), "Failed CryptAcquireContext");

	try
	{
		throwLastError(CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash), "Failed CryptCreateHash(SHA256)");
	}
	catch (...)
	{
		CryptReleaseContext(hProv, 0);
		throw;
	}
}

SHA256Hasher::~SHA256Hasher(void)
{
	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);
}

void SHA256Hasher::addData(const std::string& data)
{
	throwLastError(CryptHashData(hHash, (const BYTE*)data.c_str(), (DWORD)data.length(), 0), "Failed CryptHashData");
}

void SHA256Hasher::addData(const char* data, size_t nBytes)
{
	throwLastError(CryptHashData(hHash, (const BYTE*)data, (DWORD)nBytes, 0), "Failed CryptHashData");
}

const char* SHA256Hasher::c_str()
{
	if (result.size() == 0)
	{
		DWORD hashLength = 0;
		DWORD foo = sizeof(hashLength);
		throwLastError(CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLength, &foo, 0), "Failed CryptGetHashParam HP_HASHSIZE");

		if (hashLength > 256)
			throw std::runtime_error("hashLength is too long");
		BYTE data[256] = { 0 };

		throwLastError(CryptGetHashParam(hHash, HP_HASHVAL, data, &hashLength, 0), "Failed CryptGetHashParam HP_HASHVAL");

		for (size_t i = 0; i < hashLength; i++)
		{
			std::string s = format_string("%02x", data[i]);
			result += s;
		}
	}

	return result.c_str();
}
