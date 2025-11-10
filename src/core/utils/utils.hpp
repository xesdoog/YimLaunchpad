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


#pragma once

#include <bcrypt.h>
#include <codecvt>
#include <chrono>
#include <regex>
#include <thread>
#include <winhttp.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")


namespace YLP::Utils
{
	struct IconData
	{
		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
	};

	struct HttpResponse
	{
		std::string body;
		std::string eTag;
		int status = 0;
		bool success = false;
	};

	HttpResponse HttpRequest(
	    const std::wstring& host,
	    const std::wstring& path,
	    const std::vector<std::wstring>& headers = {},
	    const std::filesystem::path* outFile = nullptr,
	    float* outProgress = nullptr);
	
	std::string Int32ToTime(int32_t seconds);
	std::string StringToLower(const std::string& input);
	std::string FormatTime();
	std::string FormatDateShort(const std::string& iso);
	std::string FormatRelativeDate(const std::string& iso);
	std::string FormatDate(const std::string& iso);
	std::string WideToUTF8(const std::wstring& wstr);
	std::wstring UTF8ToWide(const std::string& str);
	std::optional<uint8_t> CharToHex(char const c);

	IconData HICONToRGBA(HICON hIcon); // useless

	const std::string RegexMatchHtml(const std::wstring& host, const std::wstring& path, const std::string& ptrn, const int& index = 0);
	const std::string CalcSha256(const std::filesystem::path& filePath);

	template<typename Predicate>
	static inline bool WaitUntil(Predicate pred, int timeoutMs, int pollMs)
	{
		auto start = std::chrono::steady_clock::now();
		while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeoutMs))
		{
			if (pred())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
		}
		return false;
	}
}
