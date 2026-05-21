#include "StdAfx.h"
#include "SHA256Hasher.h"
#include "format_string.h"

#include <bcrypt.h>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")

#ifndef BCRYPT_SUCCESS
#define BCRYPT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

SHA256Hasher::SHA256Hasher(void)
	: hAlg(nullptr), hHash(nullptr)
{
	NTSTATUS s = ::BCryptOpenAlgorithmProvider(
		(BCRYPT_ALG_HANDLE*)&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
	if (!BCRYPT_SUCCESS(s))
		throw std::runtime_error("BCryptOpenAlgorithmProvider(SHA256) failed");

	s = ::BCryptCreateHash((BCRYPT_ALG_HANDLE)hAlg, (BCRYPT_HASH_HANDLE*)&hHash,
		nullptr, 0, nullptr, 0, 0);
	if (!BCRYPT_SUCCESS(s))
	{
		::BCryptCloseAlgorithmProvider((BCRYPT_ALG_HANDLE)hAlg, 0);
		hAlg = nullptr;
		throw std::runtime_error("BCryptCreateHash(SHA256) failed");
	}
}

SHA256Hasher::~SHA256Hasher(void)
{
	if (hHash) ::BCryptDestroyHash((BCRYPT_HASH_HANDLE)hHash);
	if (hAlg)  ::BCryptCloseAlgorithmProvider((BCRYPT_ALG_HANDLE)hAlg, 0);
}

void SHA256Hasher::addData(const std::string& data)
{
	addData(data.c_str(), data.length());
}

void SHA256Hasher::addData(const char* data, size_t nBytes)
{
	NTSTATUS s = ::BCryptHashData((BCRYPT_HASH_HANDLE)hHash,
		(PUCHAR)data, (ULONG)nBytes, 0);
	if (!BCRYPT_SUCCESS(s))
		throw std::runtime_error("BCryptHashData failed");
}

const char* SHA256Hasher::c_str()
{
	if (result.empty())
	{
		BYTE digest[32] = { 0 };
		NTSTATUS s = ::BCryptFinishHash((BCRYPT_HASH_HANDLE)hHash,
			digest, sizeof(digest), 0);
		if (!BCRYPT_SUCCESS(s))
			throw std::runtime_error("BCryptFinishHash failed");

		for (size_t i = 0; i < sizeof(digest); ++i)
		{
			std::string s2 = format_string("%02x", digest[i]);
			result += s2;
		}
	}
	return result.c_str();
}
