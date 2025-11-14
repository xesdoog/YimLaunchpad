// Copyright (C) 2025 SAMURAI (xesdoog) & Contributors
// This file is part of YLP.
//
// YLP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// YLP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with YLP.  If not, see <https://www.gnu.org/licenses/>.


#include "utils.hpp"


namespace YLP::Utils
{
	HttpResponse HttpRequest(
	    const std::wstring& host,
	    const std::wstring& path,
	    const std::vector<std::wstring>& headers,
	    const std::filesystem::path* outFile,
	    float* outProgress)
	{
		HttpResponse response{};
		HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;

		try
		{
			hSession = WinHttpOpen(L"YLP-Client/1.0",
			    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			    WINHTTP_NO_PROXY_NAME,
			    WINHTTP_NO_PROXY_BYPASS,
			    0);
			if (!hSession)
				throw std::runtime_error("WinHttpOpen failed");

			hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
			if (!hConnect)
				throw std::runtime_error("WinHttpConnect failed");

			hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

			if (!hRequest)
				throw std::runtime_error("WinHttpOpenRequest failed");

			std::wstring headerBlock;
			for (const auto& header : headers)
				headerBlock += header;

			if (!headerBlock.empty())
				WinHttpAddRequestHeaders(hRequest, headerBlock.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

			if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
				throw std::runtime_error("WinHttpSendRequest failed");

			if (!WinHttpReceiveResponse(hRequest, nullptr))
				throw std::runtime_error("WinHttpReceiveResponse failed");

			DWORD statusCode = 0;
			DWORD dwSize = sizeof(statusCode);
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
			response.status = static_cast<int>(statusCode);

			wchar_t eTagBuffer[256] = L"";
			dwSize = sizeof(eTagBuffer);
			if (WinHttpQueryHeaders(
			        hRequest,
			        WINHTTP_QUERY_CUSTOM,
			        L"ETag",
			        eTagBuffer,
			        &dwSize,
			        WINHTTP_NO_HEADER_INDEX))
			{
				std::wstring wEtag(eTagBuffer);
				response.eTag = WideToUTF8(wEtag);
			}

			std::ofstream file;
			if (outFile)
				file.open(*outFile, std::ios::binary);

			DWORD bytesAvailable = 0;
			DWORD64 totalRead = 0, contentLength = 0;
			dwSize = sizeof(contentLength);
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &dwSize, WINHTTP_NO_HEADER_INDEX);

			std::vector<char> buffer;
			while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
			{
				std::vector<char> temp(bytesAvailable);
				DWORD bytesRead = 0;

				if (!WinHttpReadData(hRequest, temp.data(), bytesAvailable, &bytesRead) || bytesRead == 0)
					break;

				if (outFile)
					file.write(temp.data(), bytesRead);
				else
					buffer.insert(buffer.end(), temp.begin(), temp.begin() + bytesRead);

				if (outProgress && contentLength > 0)
					*outProgress = (static_cast<float>(totalRead) / static_cast<float>(contentLength));

				totalRead += bytesRead;
			}

			if (outFile)
				file.close();
			else
				response.body.assign(buffer.begin(), buffer.end());

			response.success = (response.status >= 200 && response.status < 300);

			if (outProgress)
				*outProgress = 1.0f - 0.02f * *outProgress + 0.02f * 1.0f;
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("HttpRequest exception: {}", e.what());
		}

		if (hRequest)
			WinHttpCloseHandle(hRequest);

		if (hConnect)
			WinHttpCloseHandle(hConnect);

		if (hSession)
			WinHttpCloseHandle(hSession);

		return response;
	}

	std::string StringToLower(const std::string& input)
	{
		std::string lower = input;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
		return lower;
	}

	std::string WideToUTF8(const std::wstring& wstr)
	{
		if (wstr.empty())
			return {};

		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
		std::string result(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), result.data(), sizeNeeded, nullptr, nullptr);
		return result;
	}

	std::wstring UTF8ToWide(const std::string& str)
	{
		if (str.empty())
			return L"";
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(str);
	}

	std::optional<uint8_t> CharToHex(char const c)
	{
		if (c >= '0' && c <= '9')
			return c - '0';

		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;

		return c - 'a' + 10;
	}

	std::string Int32ToTime(int32_t seconds)
	{
		if (seconds < 0)
			return "0s.";

		int hours = seconds / 3600;
		int minutes = (seconds % 3600) / 60;
		int secs = seconds % 60;

		return std::format("{:02}:{:02}:{:02}", hours, minutes, secs);
	}

	std::string FormatTime()
	{
		using namespace std::chrono;
		auto now = system_clock::now();
		std::time_t time = system_clock::to_time_t(now);
		std::tm tm;
		localtime_s(&tm, &time);

		std::ostringstream oss;
		oss << std::put_time(&tm, "%H:%M:%S");
		return oss.str();
	}

	std::string FormatDateShort(const std::string& iso)
	{
		std::tm tm{};
		std::istringstream ss(iso);
		ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

		if (ss.fail())
			return iso;

		std::ostringstream out;
		out << std::put_time(&tm, "%d %b %Y");

		return out.str();
	}

	std::string FormatRelativeDate(const std::string& iso)
	{
		std::tm tm{};
		std::istringstream ss(iso);
		ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

		if (ss.fail())
			return iso;

		auto t_c = std::mktime(&tm);
		auto now = std::time(nullptr);
		auto diff = std::difftime(now, t_c);

		const int minutes = diff / 60;
		const int hours = minutes / 60;
		const int days = hours / 24;

		if (days > 365)
			return std::to_string(days / 365) + " year(s) ago";
		else if (days > 30)
			return std::to_string(days / 30) + " month(s) ago";
		else if (days > 0)
			return std::to_string(days) + " day(s) ago";
		else if (hours > 0)
			return std::to_string(hours) + " hour(s) ago";
		else if (minutes > 0)
			return std::to_string(minutes) + " minute(s) ago";
		else
			return "just now";
	}

	std::string FormatDate(const std::string& iso)
	{
		return FormatDateShort(iso) + " (" + FormatRelativeDate(iso) + ")";
	}

	IconData HICONToRGBA(HICON hIcon)
	{
		IconData result{};

		if (!hIcon)
			return result;

		ICONINFO iconInfo{};
		if (!GetIconInfo(hIcon, &iconInfo))
			return result;

		BITMAP bmpColor{};
		GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmpColor);

		result.width = bmpColor.bmWidth;
		result.height = bmpColor.bmHeight;
		HDC hDC = GetDC(nullptr);
		HDC hMemDC = CreateCompatibleDC(hDC);
		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = bmpColor.bmWidth;
		bmi.bmiHeader.biHeight = -bmpColor.bmHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		size_t newSize = size_t(result.width * result.height * 4);
		result.pixels.resize(newSize);
		SelectObject(hMemDC, iconInfo.hbmColor);
		GetDIBits(hMemDC, iconInfo.hbmColor, 0, bmpColor.bmHeight, result.pixels.data(), &bmi, DIB_RGB_COLORS);
		DeleteDC(hMemDC);
		ReleaseDC(nullptr, hDC);

		if (iconInfo.hbmColor)
			DeleteObject(iconInfo.hbmColor);
		if (iconInfo.hbmMask)
			DeleteObject(iconInfo.hbmMask);

		return result;
	}

	const std::string RegexMatchHtml(const std::wstring& host, const std::wstring& path, const std::string& ptrn, const int& index)
	{
		if (host.empty() || path.empty())
			return {};

		auto response = HttpRequest(host, path);
		if (!response.success)
		{
			LOG_ERROR("HttpRequest failed with error code: {}", response.status);
			return {};
		}

		if (response.body.empty())
		{
			LOG_ERROR("Failed to fetch HTML!");
			return {};
		}

		std::regex pattern(ptrn);
		std::smatch match;
		if (std::regex_search(response.body, match, pattern) && match.size() > 1)
			return match[index].str();
		return {};
	}

	const std::string CalcSha256(const std::filesystem::path& filePath)
	{
		try
		{
			if (filePath.empty() || !std::filesystem::exists(filePath))
			{
				LOG_ERROR("CalcSha256: The path specified does not exist!");
				return {};
			}

			BCRYPT_ALG_HANDLE hAlg = nullptr;
			BCRYPT_HASH_HANDLE hHash = nullptr;
			NTSTATUS status = 0;
			DWORD hashObjectSize = 0, dataLen = 0;
			std::vector<uint8_t> hashObject, hash(32);

			status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
			if (!BCRYPT_SUCCESS(status))
			{
				if (hAlg)
					BCryptCloseAlgorithmProvider(hAlg, 0);
				return {};
			}

			status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectSize, sizeof(DWORD), &dataLen, 0);
			if (!BCRYPT_SUCCESS(status))
			{
				if (hAlg)
					BCryptCloseAlgorithmProvider(hAlg, 0);
				return {};
			}

			hashObject.resize(hashObjectSize);
			status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(status))
			{
				if (hHash)
					BCryptDestroyHash(hHash);

				if (hAlg)
					BCryptCloseAlgorithmProvider(hAlg, 0);
				return {};
			}

			std::ifstream file(filePath, std::ios::binary);
			if (!file.is_open())
			{
				LOG_ERROR("Failed to open file: {}", filePath.string());

				if (hHash)
					BCryptDestroyHash(hHash);

				if (hAlg)
					BCryptCloseAlgorithmProvider(hAlg, 0);
				return {};
			}

			std::vector<char> buffer(4096);
			while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0)
			{
				status = BCryptHashData(hHash, (PUCHAR)buffer.data(), (ULONG)file.gcount(), 0);
				if (!BCRYPT_SUCCESS(status))
				{
					if (hHash)
						BCryptDestroyHash(hHash);

					if (hAlg)
						BCryptCloseAlgorithmProvider(hAlg, 0);
					return {};
				}
			}

			status = BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0);
			if (!BCRYPT_SUCCESS(status))
				LOG_ERROR("CalcSHA256 failed with NTSTATUS {}", static_cast<int>(status));

			if (hHash)
				BCryptDestroyHash(hHash);

			if (hAlg)
				BCryptCloseAlgorithmProvider(hAlg, 0);

			if (file.is_open())
				file.close();

			std::ostringstream oss;
			for (auto b : hash)
				oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
			return oss.str();
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("An error has occured: {}", e.what());
			return {};
		}
	}
}
