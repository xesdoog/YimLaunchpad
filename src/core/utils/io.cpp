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

#include "io.hpp"


namespace YLP::IO
{
	void Rename(const fs::path& src, const fs::path& dest)
	{
		try
		{
			fs::rename(src, dest);
		}
		catch (const fs::filesystem_error& e)
		{
			LOG_ERROR("Error moving folder: {}", e.what());
		}
	}

	bool RemoveAll(const fs::path& target)
	{
		try
		{
			if (!fs::exists(target))
				return false;

			for (auto& entry : fs::recursive_directory_iterator(target))
			{
				const auto& p = entry.path();
				DWORD attrs = GetFileAttributesW(p.c_str());

				if (attrs == INVALID_FILE_ATTRIBUTES)
					continue;

				if (attrs & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
					SetFileAttributesW(p.c_str(), attrs & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
			}

			std::error_code ec;
			fs::remove_all(target, ec);
			if (ec)
			{
				LOG_ERROR("Failed to remove {} : {}", target.string(), ec.message());
				return false;
			}
			return true;
		}
		catch (fs::filesystem_error& e)
		{
			LOG_ERROR("Unhandled file system exception : ", e.what());
			return false;
		}
	}

	bool HasLuaFiles(const fs::path& root)
	{
		if (!fs::exists(root) || !fs::is_directory(root))
			return false;

		for (auto& entry : fs::recursive_directory_iterator(root))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".lua")
				return true;
		}
		return false;
	}

	bool FilterLuaFiles(const fs::path& root)
	{
		if (!fs::exists(root))
			return false;

		try
		{
			for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it)
			{
				const auto& path = it->path();

				if (it->is_directory())
					continue;
				else if (it->is_regular_file() && path.extension() != ".lua")
				{
					if (!fs::remove(path))
						return false;
				}
			}
			return true;
		}
		catch (const fs::filesystem_error& e)
		{
			LOG_ERROR("{}", e.what());
			return false;
		}
	}

	void Open(const std::string& path)
	{
		ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	void OpenW(const std::wstring& path)
	{
#pragma warning(suppress : 4311)
		auto result = reinterpret_cast<int>(ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));

		if (result <= 32)
		{
			std::string_view msg = "";
			switch (result)
			{
			case 0: msg = "The operating system is out of memory or resources."; break;
			case ERROR_FILE_NOT_FOUND: msg = "The specified file was not found."; break;
			case ERROR_PATH_NOT_FOUND: msg = "The specified path was not found."; break;
			case ERROR_BAD_FORMAT: msg = "The .exe file is invalid (non-Win32 app)."; break;
			case SE_ERR_ACCESSDENIED: msg = "Access denied."; break;
			case SE_ERR_ASSOCINCOMPLETE: msg = "File association incomplete."; break;
			case SE_ERR_DDEBUSY: msg = "DDE busy."; break;
			case SE_ERR_DDEFAIL: msg = "DDE failed."; break;
			case SE_ERR_DDETIMEOUT: msg = "DDE timeout."; break;
			case SE_ERR_DLLNOTFOUND: msg = "DLL not found."; break;
			case SE_ERR_NOASSOC: msg = "No association for this file type."; break;
			case SE_ERR_OOM: msg = "Out of memory."; break;
			case SE_ERR_SHARE: msg = "Sharing violation."; break;
			default: msg = "Unknown error."; break;
			}

			LOG_ERROR("Failed to perform I/O: {}", msg.data());
		}
	}

	// https://learn.microsoft.com/en-us/windows/win32/shell/common-file-dialog#ifiledialog-ifileopendialog-and-ifilesavedialog
	fs::path BrowseFile(
	    const std::vector<COMDLG_FILTERSPEC>& filters,
	    const wchar_t* title,
	    const fs::path& defaultFolder)
	{
		HRESULT hInitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		IFileOpenDialog* pFileOpen = nullptr;
		HRESULT hr = CoCreateInstance(
		    CLSID_FileOpenDialog,
		    nullptr,
		    CLSCTX_INPROC_SERVER,
		    IID_PPV_ARGS(&pFileOpen));

		if (FAILED(hr))
		{
			CoUninitialize();
			return {};
		}

		DWORD dwFlags;
		hr = pFileOpen->GetOptions(&dwFlags);
		if (FAILED(hr))
		{
			pFileOpen->Release();
			CoUninitialize();
			return {};
		}

		hr = pFileOpen->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
		if (FAILED(hr))
		{
			pFileOpen->Release();
			CoUninitialize();
			return {};
		}

		if (!filters.empty())
		{
			hr = pFileOpen->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
			if (FAILED(hr))
			{
				pFileOpen->Release();
				CoUninitialize();
				return {};
			}
		}

		if (title)
		{
			hr = pFileOpen->SetTitle(title);
			if (FAILED(hr))
			{
				pFileOpen->Release();
				CoUninitialize();
				return {};
			}
		}

		if (!defaultFolder.empty() && std::filesystem::exists(defaultFolder))
		{
			IShellItem* pFolderItem = nullptr;
			hr = SHCreateItemFromParsingName(defaultFolder.c_str(), nullptr, IID_PPV_ARGS(&pFolderItem));
			if (SUCCEEDED(hr))
			{
				pFileOpen->SetFolder(pFolderItem);
				pFolderItem->Release();
			}
		}

		hr = pFileOpen->Show(nullptr);
		if (FAILED(hr))
		{
			pFileOpen->Release();
			CoUninitialize();
			return {};
		}

		IShellItem* pItem = nullptr;
		hr = pFileOpen->GetResult(&pItem);
		if (FAILED(hr))
		{
			pFileOpen->Release();
			CoUninitialize();
			return {};
		}

		PWSTR pszFilePath = nullptr;
		hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
		if (FAILED(hr))
		{
			pItem->Release();
			pFileOpen->Release();
			CoUninitialize();
			return {};
		}

		fs::path ret(pszFilePath);
		CoTaskMemFree(pszFilePath);
		pItem->Release();
		pFileOpen->Release();
		CoUninitialize();
		return ret;
	}

	std::wstring ReadRegistryKey(HKEY rootPath, const wchar_t* subkeyPath, const wchar_t* subkeyValue)
	{
		HKEY hKey;
		LONG result = RegOpenKeyExW(rootPath, subkeyPath, 0, KEY_READ, &hKey);
		if (result != ERROR_SUCCESS)
		{
			LOG_ERROR("Failed to read registry subkey '{}'", Utils::WideToUTF8(subkeyValue).data());
			RegCloseKey(hKey);
			return {};
		}

		// std::vector<wchar_t> buffer(dataSize / sizeof(wchar_t));
		wchar_t buffer[512]{};
		DWORD dataSize = sizeof(buffer);
		std::wstring value;
		result = RegQueryValueExW(hKey, subkeyValue, NULL, NULL, reinterpret_cast<LPBYTE>(buffer), &dataSize);
		RegCloseKey(hKey);

		if (result == ERROR_SUCCESS)
			return buffer;

		LOG_ERROR("Failed to read registry value '{}'", Utils::WideToUTF8(subkeyValue).data());
		return {};
	}
}
