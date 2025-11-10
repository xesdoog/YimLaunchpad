// YLP Project - GPL-3.0
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#pragma once

#include <TlHelp32.h>
#include <Psapi.h>
#include <stdexcept>
#include <thread>
#include "pointer.hpp"


namespace YLP
{
	// Ported from YLP Python
	class ProcessScanner
	{
	public:
		explicit ProcessScanner(const std::string& processName = "");
		~ProcessScanner();

		bool FindProcess(const std::string& processName);
		bool IsProcessRunning() const;
		bool IsModuleLoaded(const std::string& moduleName);

		HANDLE GetProcessHandle() const 
		{
			return m_ProcessHandle;
		}

		DWORD GetProcessID() const
		{
			return m_Pid;
		}

		uintptr_t GetBaseAddress() const;
		size_t GetModuleSize() const;

		std::vector<uint8_t> ReadMemory(uintptr_t address, size_t size) const;
		bool IsMemoryReadable(uintptr_t address) const;
		bool IsAddressValid(uintptr_t address) const;
		void RefreshModules();

		uint64_t ScanPattern(std::vector<std::optional<uint8_t>> bytes, std::vector<uint8_t> mem);
		Pointer FindPattern(const std::string& pattern, const std::string& name = "", size_t chunkSize = 4096);

	private:
		std::optional<DWORD> GetProcessIdByName(const std::string& processName) const;
		static std::vector<std::optional<uint8_t>> ParsePattern(const std::string& sig);

		HANDLE m_ProcessHandle;
		DWORD m_Pid;
		mutable uintptr_t m_BaseAddress;

		std::string m_ProcessName;
		std::unordered_map<std::string, std::string> m_Modules;
	};
}
