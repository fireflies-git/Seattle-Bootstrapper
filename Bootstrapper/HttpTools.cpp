#include "stdafx.h"
#include "HttpTools.h"
#include "SharedHelpers.h"
#include "atlutil.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "wininet.h"
#include <sstream>
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "Ws2_32.lib")
#include <regex>
#include <system_error>

static const std::string sContentLength = "content-length: ";
static const std::string sEtag = "etag: ";

namespace HttpTools
{
	class WININETHINTERNET
	{
		HINTERNET handle;

	public:
		WININETHINTERNET(HINTERNET handle) : handle(handle) {}
		WININETHINTERNET() : handle(0) {}
		WININETHINTERNET(const WININETHINTERNET &) = delete;
		WININETHINTERNET &operator=(const WININETHINTERNET &) = delete;
		WININETHINTERNET &operator=(HINTERNET handle)
		{
			::InternetCloseHandle(this->handle);
			this->handle = handle;
			return *this;
		}
		explicit operator bool() const { return handle != 0; }
		operator HINTERNET() const { return handle; }
		~WININETHINTERNET()
		{
			::InternetCloseHandle(handle);
		}
	};

	int httpWinInet(IInstallerSite *site, const char *method, const std::string &host, const std::string &path, std::istream &input, const char *contentType, std::string &etag, std::ostream &result, bool ignoreCancel, std::function<void(int, int)> progress);
	int httpBoost(IInstallerSite *site, const char *method, const std::string &host, const std::string &path, std::istream &input, const char *contentType, std::string &etag, std::ostream &result, bool ignoreCancel, std::function<void(int, int)> progress);
	int http(IInstallerSite *site, const char *method, const std::string &host, const std::string &path, std::istream &input, const char *contentType, std::string &etag, std::ostream &result, bool ignoreCancel, std::function<void(int, int)> progress, bool log = true);

	static void dummyProgress(int, int) {}
	const std::string getPrimaryCdnHost(IInstallerSite *site)
	{
		static std::string cdnHost;
		static bool cdnHostLoaded = false;
		static bool validCdnHost = false;

		if (!cdnHostLoaded)
		{
			if (site->ReplaceCdnTxt())
			{
				std::string host = site->InstallHost();
				std::string prod("setup.seattl.lol/cdn");
				if (host.compare(prod) == 0)
				{
					cdnHost = "setup.seattl.lol/cdn";
					validCdnHost = true;
					LLOG_ENTRY1(site->Logger(), "primaryCdn: %s", cdnHost.c_str());
				}
				else
				{
					cdnHost = host;
					validCdnHost = true;
					LLOG_ENTRY1(site->Logger(), "primaryCdn: %s", cdnHost.c_str());
				}
			}
			else
			{
				try
				{
					std::ostringstream result;
					int status_code;
					std::string eTag;
					switch (status_code = httpGet(site, site->InstallHost(), "/cdn.txt", eTag, result, false, dummyProgress))
					{
					case 200:
					case 304:
						result << (char)0;
						cdnHost = result.str().c_str();
						LLOG_ENTRY1(site->Logger(), "primaryCdn: %s", cdnHost.c_str());
						validCdnHost = true;
						break;
					default:
						validCdnHost = false;
						LLOG_ENTRY1(site->Logger(), "primaryCdn failure code=%d, falling back to secondary installHost", status_code);
						break;
					}
				}
				catch (std::exception &)
				{
					// Quash exceptions, set validCdnHost to false
					validCdnHost = false;
					LLOG_ENTRY(site->Logger(), "primaryCdn exception, falling back to secondary installHost");
				}
			}

			// Only try to load the CDN once, then give up
			cdnHostLoaded = true;
		}

		return cdnHost;
	}

	const std::string getCdnHost(IInstallerSite *site)
	{
		static std::string cdnHost;
		static bool cdnHostLoaded = false;
		static bool validCdnHost = false;
		if (!cdnHostLoaded)
		{
			try
			{
				std::ostringstream result;
				std::string eTag;
				int statusCode = httpGet(site, site->BaseHost(), "/install/GetInstallerCdns.ashx", eTag, result, false, dummyProgress);
				switch (statusCode)
				{
				case 200:
				case 304:
				{
					result << (char)0;
					LLOG_ENTRY1(site->Logger(), "primaryCdns: %s", result.str().c_str());

					std::string json = result.str();
					std::regex re("\"([^\"]+)\"\\s*:\\s*([0-9]+)");
					std::smatch m;

					std::vector<std::pair<std::string, int>> cdns;
					int totalValue = 0;
					auto begin = json.cbegin();
					while (std::regex_search(begin, json.cend(), m, re))
					{
						std::string key = m[1].str();
						int value = std::stoi(m[2].str());
						if (!cdns.empty())
							cdns.emplace_back(key, cdns.back().second + value);
						else
							cdns.emplace_back(key, value);

						totalValue += value;
						begin = m.suffix().first;
					}

					if (cdns.size() && (totalValue > 0))
					{
						// randomly pick a cdn
						int r = rand() % totalValue;
						for (unsigned int i = 0; i < cdns.size(); i++)
						{
							if (r < cdns[i].second)
							{
								cdnHost = cdns[i].first;
								validCdnHost = true;
								break;
							}
						}
					}

					break;
				}
				default:
					validCdnHost = false;
					LLOG_ENTRY1(site->Logger(), "primaryCdn failure code=%d, falling back to secondary installHost", statusCode);
					break;
				}
			}
			catch (std::exception &)
			{
				LLOG_ENTRY(site->Logger(), "primaryCdn exception, falling back to secondary installHost");
			}
			// Only try to load the CDN once, then give up
			cdnHostLoaded = true;
		}

		return cdnHost;
	}

	class CInternet
	{
		HINTERNET handle;

	public:
		CInternet(HINTERNET handle) : handle(handle) {}
		CInternet() : handle(0) {}
		CInternet(const CInternet &) = delete;
		CInternet &operator=(const CInternet &) = delete;
		CInternet &operator=(HINTERNET handle)
		{
			::InternetCloseHandle(handle);
			this->handle = handle;
			return *this;
		}
		explicit operator bool() const { return handle != 0; }
		operator HINTERNET() const { return handle; }
		~CInternet()
		{
			::InternetCloseHandle(handle);
		}
	};

	class Buffer
	{
		void *const data;

	public:
		Buffer(size_t size) : data(malloc(size)) {}
		Buffer(const Buffer &) = delete;
		Buffer &operator=(const Buffer &) = delete;
		~Buffer() { free(data); }
		operator const void *() const { return data; }
		operator void *() { return data; }
		operator char *() { return (char *)data; }
	};

	int httpGet(IInstallerSite *site, std::string host, std::string path, std::string &etag, std::ostream &result, bool ignoreCancel, std::function<void(int, int)> progress, bool log)
	{
		try
		{
			std::string tmp = etag;
			std::istringstream input;
			int i = http(site, "GET", host, path, input, NULL, tmp, result, ignoreCancel, progress, log);
			etag = tmp;
			return i;
		}
		catch (silent_exception &)
		{
			throw;
		}
		catch (std::exception &e)
		{
			LLOG_ENTRY2(site->Logger(), "WARNING: First HTTP GET error for %s: %s", path.c_str(), exceptionToString(e).c_str());
			std::istringstream input;
			result.seekp(0);
			result.clear();
			// try again
			return http(site, "GET", host, path, input, NULL, etag, result, ignoreCancel, progress, log);
		}
	}

	std::string httpGetString(const std::string &url)
	{
		CUrl u;
		u.CrackUrl(convert_s2w(url).c_str());

		const bool isSecure = u.GetScheme() == ATL_URL_SCHEME_HTTPS;

		// Initialize the User Agent
		WININETHINTERNET session = InternetOpen(L"Roblox/WinInet", PRE_CONFIG_INTERNET_ACCESS, NULL, NULL, 0);
		if (!session)
		{
			throw std::runtime_error("httpGetString - InternetOpen ERROR");
		}

		WININETHINTERNET connection = ::InternetConnect(session, u.GetHostName(), u.GetPortNumber(), u.GetUserName(), u.GetPassword(), INTERNET_SERVICE_HTTP, 0, 0);
		if (!connection)
		{
			throw std::runtime_error("httpGetString - InternetConnect ERROR");
		}

		CString s = u.GetUrlPath();
		s += u.GetExtraInfo();
		WININETHINTERNET request = ::HttpOpenRequest(connection, _T("GET"), s, HTTP_VERSION, _T(""), NULL, isSecure ? INTERNET_FLAG_SECURE : 0, 0);
		if (!request)
		{
			throw std::runtime_error("httpGetString - HttpOpenRequest ERROR");
		}

		DWORD httpSendResult = ::HttpSendRequest(request, NULL, 0, 0, 0);
		if (!httpSendResult)
		{
			throw std::runtime_error("httpGetString - HttpSendRequest ERROR");
		}

		std::ostringstream data;
		while (true)
		{
			DWORD numBytes;
			if (!::InternetQueryDataAvailable(request, &numBytes, 0, 0))
			{
				DWORD err = GetLastError();
				if (err == ERROR_IO_PENDING)
				{
					Sleep(100); // we block on data.
					continue;
				}
				else
				{
					throw std::runtime_error("httpGetString - InternetQueryDataAvailable ERROR");
				}
			}

			if (numBytes == 0)
				break; // EOF

			if (numBytes == -1)
			{
				throw std::runtime_error("httpGetString - No Settings ERROR");
			}

			char *buffer = (char *)malloc(numBytes + 1);
			if (!buffer)
			{
				throw std::runtime_error("httpGetString - Failed to allocate memory for buffer");
			}

			DWORD bytesRead;
			throwLastError(::InternetReadFile(request, (LPVOID)buffer, numBytes, &bytesRead), "InternetReadFile failed");
			data.write(buffer, bytesRead);
			free(buffer);
		}

		return data.str();
	}

	int httpWinInet(IInstallerSite *site, const char *method, const std::string &host, const std::string &path, std::istream &input, const char *contentType, std::string &etag, std::ostream &result, bool ignoreCancel, std::function<void(int, int)> progress)
	{
		CUrl u;
		BOOL urlCracked;
#ifdef UNICODE
		urlCracked = u.CrackUrl(convert_s2w(host).c_str());
#else
		urlCracked = u.CrackUrl(host.c_str());
#endif

		// Initialize the User Agent
		CInternet session = InternetOpen(_T("Roblox/WinInet"), PRE_CONFIG_INTERNET_ACCESS, NULL, NULL, 0);
		if (!session)
			throw std::runtime_error(format_string("InternetOpen failed for %s https://%s%s, Error Code: %d", method, host.c_str(), path.c_str(), GetLastError()).c_str());

		CInternet connection;
		if (urlCracked)
			connection = ::InternetConnect(session, u.GetHostName(), u.GetPortNumber(), NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		else
			connection = ::InternetConnect(session, convert_s2w(host).c_str(), 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);

		if (!connection)
			throw std::runtime_error(format_string("InternetConnect failed for %s https://%s%s, Error Code: %d, Port Number: %d", method, host.c_str(), path.c_str(), GetLastError(), urlCracked ? u.GetPortNumber() : 80).c_str());

		//   1. Open HTTP Request (pass method type [get/post/..] and URL path (except server name))
		CInternet request = ::HttpOpenRequest(
			connection, convert_s2w(method).c_str(), convert_s2w(path).c_str(), NULL, NULL, NULL,
			INTERNET_FLAG_KEEP_CONNECTION |
				INTERNET_FLAG_EXISTING_CONNECT |
				INTERNET_FLAG_NEED_FILE, // ensure that it gets cached
			1);
		if (!request)
		{
			DWORD errorCode = GetLastError();

			throw std::runtime_error(format_string("HttpOpenRequest failed for %s https://%s%s, Error Code: %d", method, host.c_str(), path.c_str(), errorCode).c_str());
		}

		if (contentType)
		{
			std::string header = format_string("Content-Type: %s\r\n", contentType);
			throwLastError(::HttpAddRequestHeaders(request, convert_s2w(header).c_str(), (DWORD)header.size(), HTTP_ADDREQ_FLAG_ADD), "HttpAddRequestHeaders failed");
		}

		size_t uploadSize;
		{
			size_t x = input.tellg();
			input.seekg(0, std::ios::end);
			size_t y = input.tellg();
			uploadSize = y - x;
			input.seekg(0, std::ios::beg);
		}

		if (uploadSize == 0)
		{
			throwLastError(::HttpSendRequest(request, NULL, 0, 0, 0), "HttpSendRequest failed");
		}
		else
		{
			Buffer uploadBuffer(uploadSize);

			input.read((char *)uploadBuffer, uploadSize);

			// Send the request
			{
				INTERNET_BUFFERS buffer;
				memset(&buffer, 0, sizeof(buffer));
				buffer.dwStructSize = sizeof(buffer);
				buffer.dwBufferTotal = (DWORD)uploadSize;
				if (!HttpSendRequestEx(request, &buffer, NULL, 0, 0))
					throw std::runtime_error("HttpSendRequestEx failed");

				try
				{
					DWORD bytesWritten;
					throwLastError(::InternetWriteFile(request, uploadBuffer, (DWORD)uploadSize, &bytesWritten), "InternetWriteFile failed");

					if (bytesWritten != uploadSize)
						throw std::runtime_error("Failed to upload content");
				}
				catch (std::exception &)
				{
					::HttpEndRequest(request, NULL, 0, 0);
					throw;
				}

				throwLastError(::HttpEndRequest(request, NULL, 0, 0), "HttpEndRequest failed");
			}
		}

		// Check the return HTTP Status Code
		DWORD statusCode;
		{
			DWORD dwLen = sizeof(DWORD);
			throwLastError(::HttpQueryInfo(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &dwLen, NULL), "HttpQueryInfo HTTP_QUERY_STATUS_CODE failed");
		}
		DWORD contentLength = 0;
		{
			DWORD dwLen = sizeof(DWORD);
			::HttpQueryInfo(request, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &dwLen, NULL);
		}
		{
			CHAR buffer[256];
			DWORD dwLen = sizeof(buffer);
			if (::HttpQueryInfo(request, HTTP_QUERY_ETAG, &buffer, &dwLen, NULL))
			{
				etag = buffer;
				etag = etag.substr(1, etag.size() - 2); // remove the quotes
			}
			else
				etag.clear();
		}

		DWORD readSoFar = 0;
		while (true)
		{
			DWORD numBytes;
			if (!::InternetQueryDataAvailable(request, &numBytes, 0, 0))
				numBytes = 0;
			if (numBytes == 0)
				break; // EOF

			char buffer[1024];
			DWORD bytesRead;
			throwLastError(::InternetReadFile(request, (LPVOID)buffer, 1024, &bytesRead), "InternetReadFile failed");
			result.write(buffer, bytesRead);
			readSoFar += bytesRead;
			progress(bytesRead, contentLength);
		}

		if (statusCode != HTTP_STATUS_OK)
		{
			TCHAR buffer[512];
			DWORD length = 512;
			if (::HttpQueryInfo(request, HTTP_QUERY_STATUS_TEXT, (LPVOID)buffer, &length, 0))
				throw std::runtime_error(convert_w2s(buffer).c_str());
			else
				throw std::runtime_error(format_string("statusCode = %d", statusCode));
		}

		return statusCode;
	}

	int httpBoost(IInstallerSite* site, const char* method, const std::string& host, const std::string& path, std::istream& input, const char* contentType, std::string& etag, std::ostream& result, bool ignoreCancel, std::function<void(int, int)> progress, bool log)
	{
		if (log)
		{
			if (!etag.empty())
				LLOG_ENTRY4(site->Logger(), "%s https://%s%s If-None-Match: \"%s\"", method, host.c_str(), path.c_str(), etag.c_str());
			else
				LLOG_ENTRY3(site->Logger(), "%s https://%s%s", method, host.c_str(), path.c_str());
		}

		CUrl u;
		BOOL urlCracked;
#ifdef UNICODE
		urlCracked = u.CrackUrl(convert_s2w(host).c_str());
#else
		urlCracked = u.CrackUrl(host.c_str());
#endif

		std::string port = urlCracked ? std::to_string(u.GetPortNumber()) : "http";
		std::string hostName = urlCracked ? convert_w2s(u.GetHostName()) : host;

		// compute input size
		size_t inputSize = 0;
		{
			auto cur = input.tellg();
			if (cur != std::streampos(-1))
			{
				input.seekg(0, std::ios::end);
				auto end = input.tellg();
				if (end != std::streampos(-1))
					inputSize = static_cast<size_t>(end - cur);
				input.seekg(0, std::ios::beg);
			}
		}

		// build request string
		std::string requestStr;
		{
			std::ostringstream ss;
			ss << method << " " << path << " HTTP/1.0\r\n";
			ss << "Host: " << host << "\r\n";
			ss << "Accept: */*\r\n";
			if (inputSize || !std::strcmp(method, "POST"))
				ss << "Content-Length: " << inputSize << "\r\n";
			if (contentType)
				ss << "Content-Type: " << contentType << "\r\n";
			ss << "Connection: close\r\n";
			if (!etag.empty())
				ss << "If-None-Match: \"" << etag << "\"\r\n";
			ss << "\r\n";
			if (inputSize)
				ss << std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
			requestStr = ss.str();
		}

		// Winsock init
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			if (log)
				LLOG_ENTRY1(site->Logger(), "WSAStartup failed for %s https://%s%s", method, host.c_str(), path.c_str());
			return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
		}

		addrinfo hints{};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* addrResult = nullptr;
		int rc = getaddrinfo(hostName.c_str(), port.c_str(), &hints, &addrResult);
		if (rc != 0 || addrResult == nullptr)
		{
			if (log)
				LLOG_ENTRY2(site->Logger(), "getaddrinfo failed for %s:%s", hostName.c_str(), port.c_str());
			if (addrResult)
				freeaddrinfo(addrResult);
			WSACleanup();
			return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
		}

		SOCKET sock = INVALID_SOCKET;
		for (addrinfo* p = addrResult; p != nullptr; p = p->ai_next)
		{
			sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sock == INVALID_SOCKET)
				continue;
			if (connect(sock, p->ai_addr, (int)p->ai_addrlen) == 0)
				break;
			closesocket(sock);
			sock = INVALID_SOCKET;
		}

		freeaddrinfo(addrResult);
		if (sock == INVALID_SOCKET)
		{
			WSACleanup();
			if (log)
				LLOG_ENTRY2(site->Logger(), "Failed to connect to %s:%s - falling back to WinInet", hostName.c_str(), port.c_str());
			return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
		}

		// send request
		size_t remain = requestStr.size();
		const char* ptr = requestStr.data();
		while (remain > 0)
		{
			int s = ::send(sock, ptr, (int)remain, 0);
			if (s == SOCKET_ERROR)
			{
				closesocket(sock);
				WSACleanup();
				if (log)
					LLOG_ENTRY1(site->Logger(), "send() failed for %s", host.c_str());
				return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
			}
			ptr += s;
			remain -= s;
		}

		// receive response and parse headers
		std::string recvBuf;
		recvBuf.reserve(8192);
		std::vector<char> buf(4096);
		int status_code = 0;
		size_t contentLength = 0;
		etag.clear();
		bool headersParsed = false;

		for (;;)
		{
			int bytes = recv(sock, buf.data(), (int)buf.size(), 0);
			if (bytes > 0)
			{
				recvBuf.append(buf.data(), bytes);
				if (!headersParsed)
				{
					auto hdrEnd = recvBuf.find("\r\n\r\n");
					if (hdrEnd != std::string::npos)
					{
						// parse status line
						auto pos = recvBuf.find("\r\n");
						std::string statusLine = recvBuf.substr(0, pos);
						std::istringstream sl(statusLine);
						std::string http_ver;
						sl >> http_ver >> status_code;
						// parse headers
						std::string headers = recvBuf.substr(pos + 2, hdrEnd - (pos + 2));
						std::istringstream hs(headers);
						std::string headerLine;
						while (std::getline(hs, headerLine))
						{
							std::string lower = headerLine;
							std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
							if (lower.find(sContentLength) == 0)
								contentLength = static_cast<size_t>(std::stoul(headerLine.substr(sContentLength.size())));
							else if (lower.find(sEtag) == 0)
							{
								auto start = headerLine.find('"');
								auto end = headerLine.rfind('"');
								if (start != std::string::npos && end != std::string::npos && end > start)
									etag = headerLine.substr(start + 1, end - start - 1);
								else
									etag.clear();
							}
							if (!ignoreCancel)
								site->CheckCancel();
						}
						// write any body already received after headers
						size_t bodyStart = hdrEnd + 4;
						if (recvBuf.size() > bodyStart)
							result.write(recvBuf.data() + bodyStart, (std::streamsize)(recvBuf.size() - bodyStart));
						headersParsed = true;
						progress(static_cast<int>(result.tellp()), static_cast<int>(contentLength));
					}
				}
				else
				{
					result.write(buf.data(), bytes);
					progress(static_cast<int>(result.tellp()), static_cast<int>(contentLength));
					if (!ignoreCancel)
						site->CheckCancel();
				}
			}
			else if (bytes == 0)
			{
				break; // closed
			}
			else
			{
				closesocket(sock);
				WSACleanup();
				if (log)
					LLOG_ENTRY1(site->Logger(), "recv() failed for %s", host.c_str());
				return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
			}
		}

		closesocket(sock);
		WSACleanup();

		if (status_code == 304)
		{
			progress(1, 1);
			return status_code;
		}

		if (status_code != 200)
		{
			LLOG_ENTRY3(site->Logger(), "Trying WinInet for %s https://%s%s", method, host.c_str(), path.c_str());
			return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
		}
		{
			try
			{
				return httpBoost(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress, log);
			}
			catch (std::exception&)
			{
				LLOG_ENTRY(site->Logger(), "httpBoost failed, falling back to winInet");
				return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
			}
		}

		return status_code;
	}

	int http(IInstallerSite* site, const char* method, const std::string& host, const std::string& path, std::istream& input, const char* contentType, std::string& etag, std::ostream& result, bool ignoreCancel, std::function<void(int, int)> progress, bool log)
	{
		try
		{
			return httpBoost(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress, log);
		}
		catch (std::exception&)
		{
			LLOG_ENTRY(site->Logger(), "httpBoost failed, falling back to winInet");
			return httpWinInet(site, method, host, path, input, contentType, etag, result, ignoreCancel, progress);
		}
	}

	int httpPost(IInstallerSite * site, std::string host, std::string path, std::istream & input, const char *contentType, std::ostream &result, bool ignoreCancel, std::function<void(int, int)> progress, bool log)
	{
		std::string etag;
		return http(site, "POST", host, path, input, contentType, etag, result, ignoreCancel, progress, log);
	}

	int httpGetCdn(IInstallerSite * site, std::string secondaryHost, std::string path, std::string & etag, std::ostream & result, bool ignoreCancel, std::function<void(int, int)> progress)
	{
		std::string cdnHost;
		if (site->UseNewCdn())
			cdnHost = getCdnHost(site);

		if (cdnHost.empty())
			cdnHost = getPrimaryCdnHost(site);

		if (!cdnHost.empty())
		{
			try
			{
				std::string tmp = etag;
				int status_code = httpGet(site, cdnHost, path, tmp, result, ignoreCancel, progress);
				switch (status_code)
				{
				case 200:
				case 304:
					// We succeeded so save the etag and return success
					etag = tmp;
					return status_code;
				default:
					LLOG_ENTRY3(site->Logger(), "Failure getting '%s' from cdnHost='%s', falling back to secondaryHost='%s'", path.c_str(), cdnHost.c_str(), secondaryHost.c_str());
					// Failure of some kind, fall back to secondaryHost below
					break;
				}
			}
			catch (std::exception &)
			{
				// Trap first exception and try again with secondaryHost
			}
		}

		// Reset our result vector
		result.seekp(0);
		result.clear();

		return httpGet(site, secondaryHost, path, etag, result, ignoreCancel, progress);
	}
}
