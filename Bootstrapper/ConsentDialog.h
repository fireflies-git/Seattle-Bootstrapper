#pragma once

#include <string>

// Modal Win32 TaskDialog presenting the anti-cheat data-collection disclosure.
// The user must explicitly press Continue; closing the dialog or pressing
// Cancel is treated as a refusal. Returns true if the user consented.
class ConsentDialog
{
public:
	struct Result
	{
		bool consented;
		std::string timestampUtc;  // ISO-8601, set when consented
	};

	static Result Show();
};
