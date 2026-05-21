#include "StdAfx.h"
#include "ConsentDialog.h"

#include <Windows.h>
#include <ctime>
#include <cstdio>

namespace
{
	std::string IsoTimestampUtc()
	{
		std::time_t now = std::time(nullptr);
		std::tm utc;
#if defined(_MSC_VER)
		gmtime_s(&utc, &now);
#else
		utc = *std::gmtime(&now);
#endif
		char buf[32] = { 0 };
		std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
		return std::string(buf);
	}
}

ConsentDialog::Result ConsentDialog::Show()
{
	Result r{ false, "" };

	const wchar_t* title = L"Seattle Anti-Cheat Disclosure";
	const wchar_t* body =
		L"To maintain fair gameplay and prevent ban evasion, Seattle collects:\n\n"
		L"  \x2022  Hardware identifiers (CPU, motherboard, disk serial numbers)\n"
		L"  \x2022  Network information (hashed IP address)\n"
		L"  \x2022  Browser fingerprinting data\n\n"
		L"All sensitive information is hashed and encrypted. This data is used "
		L"solely for detecting alternate accounts and preventing abuse.\n\n"
		L"You must agree to this collection to use Seattle.\n\n"
		L"Press OK to continue. Press Cancel to exit.";

	int rc = ::MessageBoxW(nullptr, body, title,
		MB_OKCANCEL | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
	r.consented = (rc == IDOK);

	if (r.consented)
		r.timestampUtc = IsoTimestampUtc();

	return r;
}
