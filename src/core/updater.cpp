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

#include <core/gui/gui.hpp>
#include <core/gui/msgbox.hpp>
#include <core/gui/notifier.hpp>


namespace YLP
{
	void Updater::Reset()
	{
		m_DownloadProgress = 0.f;
		m_State = Idle;
		IO::RemoveAll(m_BackupPath);
	}

	Updater::Version Updater::GetLocalVersion()
	{
		if (!m_LocalVersion)
		{
			// https://learn.microsoft.com/en-us/windows/win32/api/winver/nf-winver-getfileversioninfow
			wchar_t path[MAX_PATH];
			GetModuleFileNameW(nullptr, path, MAX_PATH);

			DWORD handle;
			DWORD size = GetFileVersionInfoSizeW(path, &handle);
			if (size == 0)
				return {};

			std::vector<BYTE> data(size);
			if (!GetFileVersionInfoW(path, 0, size, data.data()))
			{
				DWORD err = GetLastError();
				LOG_ERROR("Failed to get file version information! {}\t{}", err, PsUtils::TranslateError(err));
				return {};
			}

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
		    m_ReleaseUrl.first,
		    m_ReleaseUrl.second,
		    R"(app-argument=https://github\.com/xesdoog/YLP/releases/tag/([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+))",
		    1);

		if (latest_tag.empty())
		{
#ifdef DEBUG
			LOG_DEBUG("[YLP]: HTML scrape failed! Falling back to REST API");
#endif // DEBUG

			auto response = Utils::HttpRequest(L"api.github.com", L"/repos" + m_ReleaseUrl.second);
			if (!response.success)
			{
				LOG_ERROR("[YLP]: Failed to get remote version! Please try again later.");
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
			LOG_INFO("[YLP]: Checking for updates...");
			m_State = Checking;
			Version current = GetLocalVersion();
			Version remote = GetRemoteVersion();

			if (current == Version{})
			{
				LOG_WARN("[YLP]: Local version info missing or corrupted! Skipping update check.");
				Notifier::Add("YLP", "Local version info missing or corrupted! Skipping update check.", Notifier::Warning);
				m_State = Idle;
				return;
			}

			if (remote > current)
			{
				LOG_INFO("[YLP]: Update available!");
				Notifier::Add("YLP",
				    std::format("Version {} is available! Download it from the settings tab.", remote.ToString()),
				    Notifier::Info,
				    [] {
					    GUI::SetActiveTab(ICON_SETTINGS);
					    Notifier::Close();
				    });
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

			LOG_INFO("[YLP]: No new releases found.");
			m_State = Idle;
		});
	}

	void Updater::Download()
	{
		ThreadManager::Run([this] {
			if (m_State != Pending)
			{
				LOG_ERROR("[YLP]: There are no pending updates to download!");
				return;
			}

			m_State = Downloading;
			IO::CreateFolders(m_BackupPath);
			auto response = Utils::HttpRequest(m_ReleaseUrl.first,
			    m_ReleaseUrl.second + L"/download/YLP.exe",
			    {},
			    &m_DownloadPath,
			    &m_DownloadProgress); // TODO: wrap this in a util function

			if (!response.success)
			{
				LOG_ERROR("[YLP]: Failed to download update!\tHTTP status: {}", response.status);
				Reset();
				return;
			}

			m_State = Ready;
			if (!MsgBox::Confirm(L"Confirm Update", L"Press OK to restart YLP in order to apply the update."))
			{
				LOG_INFO("[YLP]: Update canceled by user.");
				Reset();
				return;
			}

			Update();
		});
	}

	void Updater::Update()
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
		info.nShow = SW_HIDE;
		info.lpParameters = args.c_str();

		if (!ShellExecuteExA(&info))
		{
			DWORD err = GetLastError();
			LOG_ERROR("[YLP]: Failed to start updater! Please try again later.\nError code: {}\nError message: {}",
			    err,
			    PsUtils::TranslateError(err));

			IO::Remove(psPath);
			return;
		}

		PostMessageW(g_Hwnd, WM_CLOSE, 0, 0);
	}
}
