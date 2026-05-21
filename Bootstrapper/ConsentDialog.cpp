#include "StdAfx.h"
#include "ConsentDialog.h"

#include <Windows.h>
#include <commctrl.h>
#include <ctime>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

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

	const wchar_t* title = L"Seattle";
	const wchar_t* heading = L"Seattle Anti-Cheat Disclosure";
	const wchar_t* body =
		L"To maintain fair gameplay and prevent ban evasion, Seattle collects:\n\n"
		L"  \x2022  Hardware identifiers (CPU, motherboard, disk serial numbers)\n"
		L"  \x2022  Network information (hashed IP address)\n"
		L"  \x2022  Browser fingerprinting data\n\n"
		L"All sensitive information is hashed and encrypted. This data is used "
		L"solely for detecting alternate accounts and preventing abuse.\n\n"
		L"You must agree to this collection to use Seattle.";

	TASKDIALOGCONFIG cfg = { 0 };
	cfg.cbSize = sizeof(cfg);
	cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
	cfg.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
	cfg.pszWindowTitle = title;
	cfg.pszMainIcon = TD_SHIELD_ICON;
	cfg.pszMainInstruction = heading;
	cfg.pszContent = body;

	int button = 0;
	HRESULT hr = ::TaskDialogIndirect(&cfg, &button, nullptr, nullptr);
	if (FAILED(hr))
	{
		// Fall back to plain MessageBox if TaskDialogIndirect isn't available.
		int rc = ::MessageBoxW(nullptr, body, heading, MB_OKCANCEL | MB_ICONINFORMATION);
		r.consented = (rc == IDOK);
	}
	else
	{
		r.consented = (button == IDOK);
	}

	if (r.consented)
		r.timestampUtc = IsoTimestampUtc();

	return r;
}
