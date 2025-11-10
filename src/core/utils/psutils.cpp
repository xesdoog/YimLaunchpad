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


#include "psutils.hpp"


namespace YLP::PsUtils
{
	InjectResult InjectResult::Ok()
	{
		InjectResult r;
		r.success = true;
		r.message = "OK";
		return r;
	}

	InjectResult InjectResult::Err(std::string msg, DWORD err)
	{
		InjectResult r;
		r.success = false;
		r.message = std::move(msg);
		r.win_error = err;
		return r;
	}

	void ProcessList::StartUpdating()
	{
		if (m_Running)
			return;

		m_Running = true;

		ThreadManager::RunDetached([this]() {
			while (m_Running)
			{
				UpdateProcesses();
				std::unique_lock lock(m_CVMutex);
				m_ConVar.wait_for(lock, 2s, [this] {
					return !m_Running;
				});
			}
		});
	}

	void ProcessList::StopUpdating()
	{
		if (!m_Running)
			return;

		m_Running = false;
		m_ConVar.notify_all();
	}

	std::vector<ProcessEntry> ProcessList::GetSnapshot()
	{
		std::scoped_lock lock(m_Mutex);
		return m_Processes;
	}

	void ProcessList::UpdateProcesses()
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return;

		PROCESSENTRY32W entry{};
		entry.dwSize = sizeof(entry);

		std::vector<ProcessEntry> tempList;

		if (Process32FirstW(snapshot, &entry))
		{
			do
			{
				char name[260];
				WideCharToMultiByte(CP_UTF8, 0, entry.szExeFile, -1, name, sizeof(name), nullptr, nullptr);

				ProcessEntry proc;
				proc.m_Name = name;
				proc.m_Pid = entry.th32ProcessID;
				// proc.m_Icon = GetProcessIconTexture(entry.th32ProcessID, name);
				tempList.push_back(proc);
			} while (Process32NextW(snapshot, &entry));
		}

		CloseHandle(snapshot);
		std::scoped_lock lock(m_Mutex);
		m_Processes.swap(tempList);
	}

	std::optional<DWORD> WaitForProcessExit(HANDLE hProc, DWORD timeoutMs)
	{
		if (!hProc || hProc == INVALID_HANDLE_VALUE)
		{
			LOG_ERROR("WaitForProcessExit: invalid handle: 0x{:X}", (uintptr_t)hProc);
			return std::nullopt;
		}

		DWORD wait = WaitForSingleObject(hProc, timeoutMs);
		if (wait == WAIT_OBJECT_0)
		{
			DWORD exitCode = STILL_ACTIVE;
			if (!GetExitCodeProcess(hProc, &exitCode))
			{
				DWORD last = GetLastError();
				LOG_ERROR("GetExitCodeProcess failed: 0x{:X} ({})", last, TranslateError(last));
				return std::nullopt;
			}
			return exitCode;
		}
		else if (wait == WAIT_TIMEOUT)
		{
			return std::nullopt;
		}
		else
		{
			DWORD last = GetLastError();
			LOG_ERROR("WaitForSingleObject failed: 0x{:X} ({})", last, TranslateError(last));
			return std::nullopt;
		}
	}

	DllInfo ValidateDLL(const std::filesystem::path& file)
	{
		DllInfo info{};
		if (!std::filesystem::exists(file))
		{
			info.error = "File not found";
			return info;
		}

		HANDLE hFile = CreateFileW(file.wstring().c_str(),
		    GENERIC_READ,
		    FILE_SHARE_READ,
		    nullptr,
		    OPEN_EXISTING,
		    FILE_ATTRIBUTE_NORMAL,
		    nullptr);

		if (hFile == INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);
			info.error = "CreateFile failed";
			return info;
		}

		HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (!hMap)
		{
			CloseHandle(hFile);
			info.error = "CreateFileMapping failed";
			return info;
		}

		LPVOID base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
		if (!base)
		{
			info.error = "MapViewOfFile failed";
			CloseHandle(hMap);
			CloseHandle(hFile);
			return info;
		}

		auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		{
			info.error = "Invalid DOS signature";
			UnmapViewOfFile(base);
			CloseHandle(hMap);
			CloseHandle(hFile);
			return info;
		}

		auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>((BYTE*)base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
		{
			info.error = "Invalid NT signature";
			UnmapViewOfFile(base);
			CloseHandle(hMap);
			CloseHandle(hFile);
			return info;
		}

		info.is64bit = (nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
		const auto& opt = nt->OptionalHeader;
		if (opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size > 0 && opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0)
		{
			info.has_exports = true;
		}

		info.ok = true;
		UnmapViewOfFile(base);
		CloseHandle(hMap);
		CloseHandle(hFile);
		return info;
	}

	DllInfo AddDLL()
	{
		const std::vector<COMDLG_FILTERSPEC> filters = {{L"DLL (*.dll)", L"*.dll"}};
		std::filesystem::path dllPath = IO::BrowseFile(filters, L"Select a DLL");
		if (dllPath.empty())
			return {};

		DllInfo info = ValidateDLL(dllPath);
		info.checksum = Utils::CalcSha256(dllPath);
		info.filepath = dllPath;
		info.name = dllPath.filename().string();
		return info;
	}

	int GetProcessId(std::string_view name)
	{
		if (name.empty())
			return -1;

		int req = MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), nullptr, 0);
		if (req <= 0)
			return -1;

		std::vector<wchar_t> wbuf(req + 1);
		MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), wbuf.data(), req);
		wbuf[req] = L'\0';

		ScopedHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
		if (!snap)
			return -1;

		PROCESSENTRY32W pe{};
		pe.dwSize = sizeof(pe);

		if (!Process32FirstW(snap.Get(), &pe))
			return -1;

		do
		{
			if (_wcsicmp(pe.szExeFile, wbuf.data()) == 0)
				return static_cast<int>(pe.th32ProcessID);
		} while (Process32NextW(snap.Get(), &pe));

		return -1;
	}

	const bool IsSameArch(HANDLE hTargetProcess)
	{
		BOOL targetIsWow = FALSE;
		if (!IsWow64Process(hTargetProcess, &targetIsWow))
			return false;

		BOOL selfIsWow = FALSE;
		if (!IsWow64Process(GetCurrentProcess(), &selfIsWow))
			return false;

		return (targetIsWow == selfIsWow);
	}

	HANDLE RemoteLoadLibraryW(HANDLE hProcess, LPVOID lpRemoteWstr)
	{
		HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
		if (!hKernel32)
			return nullptr;

		FARPROC proc = GetProcAddress(hKernel32, "LoadLibraryW");
		if (!proc)
			return nullptr;

		return CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(proc), lpRemoteWstr, 0, nullptr);
	}

	InjectResult Inject(std::string_view processName, std::filesystem::path dllPath)
	{
		auto dllInfo = ValidateDLL(dllPath);
		if (!dllInfo.ok)
			return InjectResult::Err(std::string("PE validation failed: ") + dllInfo.error);

		if (!dllPath.is_absolute())
			dllPath = std::filesystem::absolute(dllPath);

		const int pid = GetProcessId(processName);
		if (pid == -1)
			return InjectResult::Err(std::string("Process not found: ") + std::string(processName));

		const DWORD desiredAccess = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;
		ScopedHandle hProcess(OpenProcess(desiredAccess, FALSE, static_cast<DWORD>(pid)));
		if (!hProcess)
			return InjectResult::Err("OpenProcess failed", GetLastError());

		if (!IsSameArch(hProcess.Get()))
			return InjectResult::Err("Process bitness mismatch (injector and target must be same architecture).");

		std::wstring dllW = dllPath.wstring();
		const SIZE_T bytes = (dllW.size() + 1) * sizeof(wchar_t);

		LPVOID remoteMem = VirtualAllocEx(hProcess.Get(), nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!remoteMem)
			return InjectResult::Err("VirtualAllocEx failed", GetLastError());

		SIZE_T written = 0;
		if (!WriteProcessMemory(hProcess.Get(), remoteMem, dllW.c_str(), bytes, &written) || written != bytes)
		{
			const DWORD err = GetLastError();
			VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
			return InjectResult::Err("WriteProcessMemory failed", err);
		}

		ScopedHandle hThread(RemoteLoadLibraryW(hProcess.Get(), remoteMem));
		if (!hThread)
		{
			const DWORD err = GetLastError();
			VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
			return InjectResult::Err("CreateRemoteThread (LoadLibraryW) failed", err);
		}

		const DWORD wait = WaitForSingleObject(hThread.Get(), 10'000);
		if (wait == WAIT_FAILED)
		{
			const DWORD err = GetLastError();
			LOG_WARN("WaitForSingleObject failed: {}", std::system_category().message(err));
		}
		else if (wait == WAIT_TIMEOUT)
		{
			LOG_WARN("Remote thread timed out after 10 seconds.");
		}

		DWORD exitCode = 0;
		if (!GetExitCodeThread(hThread.Get(), &exitCode))
		{
			const DWORD err = GetLastError();
			VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
			return InjectResult::Err("GetExitCodeThread failed", err);
		}

		if (exitCode == 0)
		{
			VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
			return InjectResult::Err("Remote LoadLibraryW returned NULL (load failed inside target).");
		}

		if (!VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE))
		{
			LOG_WARN("VirtualFreeEx failed during cleanup: {}", std::system_category().message(GetLastError()));
		}

		char buf[64];
		sprintf_s(buf, "Injected successfully. Remote module handle: 0x%08X", static_cast<unsigned int>(exitCode));
		LOG_INFO("{}", buf);
		return InjectResult::Ok();
	}

	std::string TranslateError(DWORD exitCode)
	{
		LPWSTR msgBuffer = nullptr;
		DWORD result = FormatMessageW(
		    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		    nullptr,
		    exitCode,
		    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		    reinterpret_cast<LPWSTR>(&msgBuffer),
		    0,
		    nullptr);

		std::string errorMsg;
		if (result != 0 && msgBuffer != nullptr)
		{
			errorMsg = Utils::WideToUTF8(msgBuffer);
			LocalFree(msgBuffer);

			if (auto lastPos = errorMsg.find_last_not_of(" \r\n"); lastPos != std::string::npos)
				errorMsg.erase(lastPos + 1);
		}
		else
		{
			errorMsg = "UNKNOWN_ERROR";
		}

		return errorMsg;
	}

	const bool IsServiceRunning(const std::wstring& serviceName)
	{
		SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
		if (!scm)
		{
			LOG_DEBUG("OpenSCManager failed: {}", GetLastError());
			return false;
		}

		SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_STATUS);
		if (!svc)
		{
			CloseServiceHandle(scm);
			return false;
		}

		SERVICE_STATUS_PROCESS ssp{};
		DWORD bytesNeeded = 0;
		BOOL ok = QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded);
		if (!ok)
		{
			CloseServiceHandle(svc);
			CloseServiceHandle(scm);
			return false;
		}

		bool running = (ssp.dwCurrentState == SERVICE_RUNNING);
		CloseServiceHandle(svc);
		CloseServiceHandle(scm);
		return running;
	}

	BOOL CALLBACK EnumProcessWindows(HWND hwnd, LPARAM lParam)
	{
		EnumWindowData& ewd = *reinterpret_cast<EnumWindowData*>(lParam);
		DWORD dwPid = 0;

		GetWindowThreadProcessId(hwnd, &dwPid);

		if (dwPid == ewd.m_Pid)
		{
			ewd.m_HWND = hwnd;
			return FALSE;
		}
		return TRUE;
	}

	HWND GetHwndFromPid(DWORD pid)
	{
		EnumWindowData ewd{0};
		ewd.m_Pid = pid;
		EnumWindows(EnumProcessWindows, reinterpret_cast<LPARAM>(&ewd));
		return ewd.m_HWND;
	}
}
