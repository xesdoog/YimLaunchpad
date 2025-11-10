// YLP Project - GPL-3.0
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#pragma once

#include <combaseapi.h>
#include <shellapi.h>
#include <shobjidl.h>


namespace fs = std::filesystem;

namespace YLP::IO
{
	inline fs::path Path(const fs::path& _path)
	{
		return fs::path(_path);
	}

	inline bool Exists(const fs::path& _path)
	{
		return fs::exists(_path);
	}

	inline bool CreateFolder(const fs::path& _path)
	{
		return fs::create_directory(_path);
	}

	inline bool CreateFolders(const fs::path& _path)
	{
		return fs::create_directories(_path);
	}

	inline bool Remove(const fs::path& _path)
	{
		return fs::remove(_path);
	}

	void Rename(const fs::path& src, const fs::path& dest);
	bool RemoveAll(const fs::path& target);
	bool HasLuaFiles(const fs::path& root);
	bool FilterLuaFiles(const fs::path& root);
	void Open(const std::string& path);
	void OpenW(const std::wstring& path);

	std::wstring ReadRegistryKey(HKEY rootPath, const wchar_t* subkeyPath, const wchar_t* subkeyValue);

	fs::path BrowseFile(
	    const std::vector<COMDLG_FILTERSPEC>& filters = {},
	    const wchar_t* title = L"Select a file",
	    const std::filesystem::path& defaultFolder = {});
}
