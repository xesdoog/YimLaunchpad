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


#include "updater.hpp"

#pragma comment(lib, "Version.lib")


namespace YLP
{
	Updater::Version Updater::GetLocalVersion()
	{
		if (!m_LocalVersion)
		{
			wchar_t path[MAX_PATH];
			GetModuleFileNameW(nullptr, path, MAX_PATH);

			DWORD handle;
			DWORD size = GetFileVersionInfoSizeW(path, &handle);
			if (size == 0)
				return {};

			std::vector<BYTE> data(size);
			if (!GetFileVersionInfoW(path, handle, size, data.data()))
				return {};

			VS_FIXEDFILEINFO* ffi = nullptr;
			UINT len = 0;
			if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&ffi), &len))
				return {};

			m_LocalVersion = {
			    static_cast<uint16_t>(HIWORD(ffi->dwFileVersionMS)),
			    static_cast<uint16_t>(LOWORD(ffi->dwFileVersionMS)),
			    static_cast<uint16_t>(HIWORD(ffi->dwFileVersionLS)),
			    static_cast<uint16_t>(LOWORD(ffi->dwFileVersionLS))};
		}

		return m_LocalVersion;
	}

	Updater::Version Updater::GetRemoteVersion()
	{
		std::string latest_tag = Utils::RegexMatchHtml(
		    L"github.com",
		    L"/xesdoog/YLP/releases/latest",
		    R"(app-argument=https://github\.com/xesdoog/YLP/releases/tag/([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+))",
		    1);

		if (latest_tag.empty())
		{

#ifdef DEBUG
			LOG_DEBUG("HTML scrape failed! Falling back to REST API");
#endif // DEBUG

			auto response = Utils::HttpRequest(L"api.github.com", L"/repos/xesdoog/YLP/releases/latest");
			if (!response.success)
			{
				LOG_ERROR("Failed to get remote version! Please try again later.");
				m_State = Idle;
				return {};
			}

			latest_tag = response.body;
		}

		Version v;
		swscanf_s(Utils::UTF8ToWide(latest_tag).c_str(), L"%d.%d.%d.%d", &v.major, &v.minor, &v.patch, &v.build);
		return v;
	}

	void Updater::Check()
	{
		ThreadManager::Run([this] {
			m_State = Checking;
			Version current = GetLocalVersion();
			Version remote = GetRemoteVersion();

			if (current == Version{})
			{
				LOG_WARN("Local version info missing or corrupted! Skipping update check.");
				m_State = Idle;
				return;
			}

			if (remote > current)
			{
				LOG_INFO("Update available!");
				m_State = Pending;
				return;
			}
#ifndef DEBUG
			if (remote < current)
			{
				LOG_ERROR("Version mismatch! Are you a dev or a skid?");
				m_State = Error;
			}
#endif // !DEBUG

			LOG_INFO("No new releases found. YLP is up to date.");
			m_State = Idle;
		});
	}

	void Updater::Download()
	{
		ThreadManager::Run([this] {
			if (m_State != Pending)
			{
				LOG_ERROR("There are no pending updates to download!");
				return;
			}

			m_State = Downloading;
			std::wstring urlHost = L"github.com";
			std::wstring urlPath = L"/xesdoog/YLP/releases/latest/download/YLP.exe";
			std::filesystem::path backupPath = g_ProjectPath / "update_cache";
			std::filesystem::path exePath = g_ProjectPath / "update_cache" / "YLP.exe";

			IO::CreateFolders(backupPath);
			auto response = Utils::HttpRequest(urlHost, urlPath, {}, &exePath, &m_DownloadProgress); // TODO: wrap this in a util function
			if (!response.success)
			{
				LOG_ERROR("Failed to download update!\tHTTP status: {}", response.status);
				m_DownloadProgress = 0.f;
				m_State = Idle;
				IO::RemoveAll(backupPath);
				return;
			}

			m_State = Ready;
			if (!MsgBox::Confirm(L"Confirm Update", L"Press OK to restart YLP in order to apply the update."))
			{
				IO::RemoveAll(backupPath);
				LOG_INFO("Update canceled by user.");
				m_DownloadProgress = 0.f;
				m_State = Idle;
				return;
			}

			Start();
		});
	}

	void Updater::Start()
	{
		const std::filesystem::path psPath = g_ProjectPath / "YLPUpdater.ps1";
		char exeBuffer[MAX_PATH];
		GetModuleFileNameA(nullptr, exeBuffer, MAX_PATH);
		const std::filesystem::path odlExePath = exeBuffer;
		const std::string psScript = R"(param([string]$ExePath)
$CurrentDir = Split-Path -Parent $ExePath
$ExeName = Split-Path -Leaf $ExePath
$CacheDir = Join-Path $PSScriptRoot "update_cache"
$BackupDir = Join-Path $PSScriptRoot "update_backup"

New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null

$TempFile = Join-Path $CacheDir $ExeName
$BackupFile = Join-Path $BackupDir "$ExeName.bak"

Write-Host "[Updater] Replacing with new version..."
Move-Item -Path $ExePath -Destination $BackupFile -Force
Move-Item -Path $TempFile -Destination $ExePath -Force

Write-Host "[Updater] Cleaning up..."
Remove-Item -Recurse -Force $CacheDir
Remove-Item -Recurse -Force $BackupDir

Start-Process -FilePath $ExePath
Remove-Item -Path $MyInvocation.MyCommand.Path -Force
exit 0
)";

		std::ofstream out(psPath);
		out << psScript;
		out.close();

		std::string args = "-ExecutionPolicy Bypass -File \""
			+ psPath.string()
			+ "\" -ExePath \""
			+ odlExePath.string()
			+ "\" -NoProfile -WindowStyle Hidden";

		SHELLEXECUTEINFOA info{};
		info.cbSize = sizeof(info);
		info.fMask = SEE_MASK_NOCLOSEPROCESS;
		info.lpVerb = "open";
		info.lpFile = "powershell.exe";
		info.lpParameters = args.c_str();
		info.nShow = SW_HIDE;

		if (!ShellExecuteExA(&info))
		{
			DWORD err = GetLastError();
			LOG_ERROR("Failed to start updater! Please try again later.\nError code: {}\nError message: {}",
			    err,
			    PsUtils::TranslateError(err));

			IO::Remove(psPath);
			return;
		}

		PostMessageW(g_Hwnd, WM_CLOSE, 0, 0);
	}
}
