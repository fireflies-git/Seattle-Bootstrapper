#include "StdAfx.h"
#include "HardwareFingerprint.h"
#include "SHA256Hasher.h"
#include "FileSystem.h"

#include <comdef.h>
#include <Wbemidl.h>
#include <wincrypt.h>
#include <dpapi.h>
#include <iphlpapi.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace
{
	std::string Trim(std::string s)
	{
		auto notSpace = [](unsigned char c) { return !std::isspace(c); };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
		return s;
	}

	std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	std::string BstrToString(const BSTR bstr)
	{
		if (!bstr) return std::string();
		int len = ::SysStringLen(bstr);
		int need = ::WideCharToMultiByte(CP_UTF8, 0, bstr, len, nullptr, 0, nullptr, nullptr);
		if (need <= 0) return std::string();
		std::string out(need, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, bstr, len, out.data(), need, nullptr, nullptr);
		return out;
	}

	// Single-property WMI query. Returns the first non-empty result, or "".
	std::string WmiQueryFirst(IWbemServices* svc, const wchar_t* wql, const wchar_t* prop)
	{
		if (!svc) return std::string();
		IEnumWbemClassObject* en = nullptr;
		HRESULT hr = svc->ExecQuery(
			bstr_t("WQL"), bstr_t(wql),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
		if (FAILED(hr) || !en) return std::string();

		std::string result;
		IWbemClassObject* obj = nullptr;
		ULONG returned = 0;
		while (en->Next(WBEM_INFINITE, 1, &obj, &returned) == S_OK && returned == 1)
		{
			VARIANT v;
			VariantInit(&v);
			if (SUCCEEDED(obj->Get(prop, 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR && v.bstrVal)
			{
				auto s = Trim(BstrToString(v.bstrVal));
				if (!s.empty())
				{
					result = s;
					VariantClear(&v);
					obj->Release();
					break;
				}
			}
			VariantClear(&v);
			obj->Release();
			obj = nullptr;
		}
		en->Release();
		return result;
	}

	std::string RegRead(HKEY root, const wchar_t* sub, const wchar_t* name)
	{
		HKEY h = nullptr;
		if (::RegOpenKeyExW(root, sub, 0, KEY_READ | KEY_WOW64_64KEY, &h) != ERROR_SUCCESS)
			return std::string();
		wchar_t buf[512] = { 0 };
		DWORD bytes = sizeof(buf) - sizeof(wchar_t);
		DWORD type = 0;
		LONG rc = ::RegQueryValueExW(h, name, nullptr, &type, (LPBYTE)buf, &bytes);
		::RegCloseKey(h);
		if (rc != ERROR_SUCCESS) return std::string();
		int need = ::WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
		if (need <= 1) return std::string();
		std::string out(need - 1, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, buf, -1, out.data(), need, nullptr, nullptr);
		return Trim(out);
	}

	std::string PrimaryMacAddress()
	{
		ULONG size = 0;
		if (::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
			return std::string();
		std::vector<BYTE> buf(size);
		auto* head = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
		if (::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, head, &size) != NO_ERROR)
			return std::string();

		for (auto* a = head; a; a = a->Next)
		{
			if (a->IfType != IF_TYPE_ETHERNET_CSMACD && a->IfType != IF_TYPE_IEEE80211) continue;
			if (a->OperStatus != IfOperStatusUp) continue;
			if (a->PhysicalAddressLength == 0) continue;

			char out[32] = { 0 };
			snprintf(out, sizeof(out), "%02x:%02x:%02x:%02x:%02x:%02x",
				a->PhysicalAddress[0], a->PhysicalAddress[1], a->PhysicalAddress[2],
				a->PhysicalAddress[3], a->PhysicalAddress[4], a->PhysicalAddress[5]);
			return std::string(out);
		}
		return std::string();
	}

	// %LOCALAPPDATA%\Seattle\hwfp.dat
	std::wstring CachePath()
	{
		auto dir = FileSystem::getSpecialFolder(FileSystem::RobloxUserApplicationData, true);
		return dir + L"hwfp.dat";
	}

	bool DpapiProtect(const std::string& plaintext, std::vector<BYTE>& out)
	{
		DATA_BLOB in = { (DWORD)plaintext.size(), (BYTE*)plaintext.data() };
		DATA_BLOB result = { 0, nullptr };
		BOOL ok = ::CryptProtectData(&in, L"SeattleHWFP", nullptr, nullptr, nullptr,
			CRYPTPROTECT_LOCAL_MACHINE, &result);
		if (!ok) return false;
		out.assign(result.pbData, result.pbData + result.cbData);
		::LocalFree(result.pbData);
		return true;
	}
}

HardwareFingerprint::Components HardwareFingerprint::Collect()
{
	Components c;

	IWbemLocator* loc = nullptr;
	IWbemServices* svc = nullptr;
	HRESULT hr = ::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID*)&loc);
	if (SUCCEEDED(hr) && loc)
	{
		hr = loc->ConnectServer(bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
			0, nullptr, nullptr, &svc);
		if (SUCCEEDED(hr) && svc)
		{
			::CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
				RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

			c.cpuId       = WmiQueryFirst(svc, L"SELECT ProcessorId FROM Win32_Processor", L"ProcessorId");
			c.mbSerial    = WmiQueryFirst(svc, L"SELECT SerialNumber FROM Win32_BaseBoard", L"SerialNumber");
			c.biosSerial  = WmiQueryFirst(svc, L"SELECT SerialNumber FROM Win32_BIOS", L"SerialNumber");
			c.systemUuid  = WmiQueryFirst(svc, L"SELECT UUID FROM Win32_ComputerSystemProduct", L"UUID");
			c.diskSerial  = WmiQueryFirst(svc, L"SELECT SerialNumber FROM Win32_DiskDrive WHERE MediaType LIKE 'Fixed%'", L"SerialNumber");
			c.gpuDeviceId = WmiQueryFirst(svc, L"SELECT PNPDeviceID FROM Win32_VideoController", L"PNPDeviceID");

			svc->Release();
		}
		loc->Release();
	}

	c.macAddress = PrimaryMacAddress();

	// Registry fallbacks.
	if (c.biosSerial.empty())
		c.biosSerial = RegRead(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardManufacturer");
	if (c.systemUuid.empty())
		c.systemUuid = RegRead(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid");

	auto note = [&](const std::string& field, const std::string& value) {
		if (value.empty()) c.missing.push_back(field);
	};
	note("cpuId", c.cpuId);
	note("mbSerial", c.mbSerial);
	note("biosSerial", c.biosSerial);
	note("systemUuid", c.systemUuid);
	note("diskSerial", c.diskSerial);
	note("macAddress", c.macAddress);
	note("gpuDeviceId", c.gpuDeviceId);

	return c;
}

std::string HardwareFingerprint::ComputeHash(const Components& c)
{
	auto canon = [](const std::string& s) { return ToLower(Trim(s)); };

	std::ostringstream oss;
	oss << canon(c.cpuId) << "|"
		<< canon(c.mbSerial) << "|"
		<< canon(c.biosSerial) << "|"
		<< canon(c.systemUuid) << "|"
		<< canon(c.diskSerial) << "|"
		<< canon(c.macAddress) << "|"
		<< canon(c.gpuDeviceId);

	SHA256Hasher h;
	const std::string payload = oss.str();
	h.addData(payload);
	return h.toString();
}

std::string HardwareFingerprint::CollectAndHashCached()
{
	auto components = Collect();
	auto hash = ComputeHash(components);
	if (hash.empty()) return hash;

	try
	{
		std::vector<BYTE> protectedBytes;
		if (DpapiProtect(hash, protectedBytes))
		{
			std::wstring path = CachePath();
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			if (out.is_open())
			{
				out.write(reinterpret_cast<const char*>(protectedBytes.data()),
					(std::streamsize)protectedBytes.size());
			}
		}
	}
	catch (...) { /* cache write is best-effort */ }

	return hash;
}
