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

#include <condition_variable>
#include <shellapi.h>
#include <system_error>
#include <TlHelp32.h>
#include <thread>


namespace YLP::PsUtils
{
	struct ProcessEntry
	{
		std::string m_Name;
		DWORD m_Pid = 0;
		ImTextureID m_Icon = NULL; // fix it or ditch it
	};

	struct InjectResult
	{
		bool success = false;
		std::string message;
		DWORD win_error = 0;

		static InjectResult Ok();
		static InjectResult Err(std::string msg, DWORD err = 0);
	};

	struct DllInfo
	{
		bool ok = false;
		bool is64bit = false;
		bool has_exports = false;
		std::filesystem::path filepath;
		std::string error;
		std::string checksum;
		std::string name;
	};

	struct EnumWindowData
	{
		DWORD m_Pid;
		HWND m_HWND;
	};

	class ProcessList : public Singleton<ProcessList>
	{
		friend class Singleton<ProcessList>;

	public:
		~ProcessList()
		{
			StopUpdating();
		}

		void StartUpdating();
		void StopUpdating();
		std::vector<ProcessEntry> GetSnapshot();

	private:
		void UpdateProcesses();

		std::vector<ProcessEntry> m_Processes;
		std::mutex m_Mutex;
		std::thread m_Worker;
		std::atomic<bool> m_Running{false};
		std::condition_variable m_ConVar;
		std::mutex m_CVMutex;
	};

	class ScopedHandle
	{
		HANDLE m_Handle = INVALID_HANDLE_VALUE;

	public:
		ScopedHandle() = default;
		explicit ScopedHandle(HANDLE h) :
		    m_Handle(h)
		{
		}
		~ScopedHandle()
		{
			Close();
		}

		bool IsValid() const noexcept
		{
			return m_Handle != nullptr && m_Handle != INVALID_HANDLE_VALUE;
		}

		HANDLE Get() const noexcept
		{
			return m_Handle;
		}

		void Close() noexcept
		{
			if (IsValid())
				CloseHandle(m_Handle);
			m_Handle = INVALID_HANDLE_VALUE;
		}

		explicit operator bool() const noexcept
		{
			return IsValid();
		}
	};

	int GetProcessId(std::string_view name);
	const bool IsSameArch(HANDLE hTargetProcess);
	const bool IsServiceRunning(const std::wstring& serviceName);
	DllInfo AddDLL();
	DllInfo ValidateDLL(const std::filesystem::path& file);
	HANDLE RemoteLoadLibraryW(HANDLE hProcess, LPVOID lpRemoteWstr);
	InjectResult Inject(std::string_view processName, std::filesystem::path dllPath);
	std::string TranslateError(DWORD exitCode);
	std::optional<DWORD> WaitForProcessExit(HANDLE hProc, DWORD timeoutMs);
	BOOL CALLBACK EnumProcessWindows(HWND hWnd, LPARAM lParam);
	HWND GetHwndFromPid(DWORD pid);
}
