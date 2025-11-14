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


#include "scanner.hpp"


namespace YLP
{
	ProcessScanner::ProcessScanner(const std::string& processName) :
	    m_ProcessHandle(nullptr),
	    m_Pid(0),
	    m_BaseAddress(0),
	    m_ProcessName(processName)
	{
		if (!processName.empty())
			FindProcess(processName);
	}

	ProcessScanner::~ProcessScanner()
	{
		if (m_ProcessHandle)
			CloseHandle(m_ProcessHandle);
	}

	std::optional<DWORD> ProcessScanner::GetProcessIdByName(const std::string& processName) const
	{
		return PsUtils::GetProcessId(processName);
	}

	bool ProcessScanner::FindProcess(const std::string& processName)
	{
		auto pidOpt = GetProcessIdByName(processName);
		if (!pidOpt.has_value())
			return false;

		m_Pid = pidOpt.value();
		m_ProcessName = processName;
		m_ProcessHandle = OpenProcess(SYNCHRONIZE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, m_Pid);

		if (!m_ProcessHandle)
			return false;

		RefreshModules();
		return true;
	}

	bool ProcessScanner::IsProcessRunning() const
	{
		if (!m_ProcessHandle)
			return false;

		DWORD exitCode = 0;
		if (!GetExitCodeProcess(m_ProcessHandle, &exitCode))
			return false;

		return exitCode == STILL_ACTIVE;
	}

	void ProcessScanner::RefreshModules()
	{
		m_Modules.clear();

		HMODULE hMods[1024];
		DWORD cbNeeded;

		if (EnumProcessModulesEx(m_ProcessHandle, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL))
		{
			for (unsigned i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i)
			{
				char modName[MAX_PATH];
				if (GetModuleFileNameExA(m_ProcessHandle, hMods[i], modName, sizeof(modName)))
				{
					std::string baseName = Utils::StringToLower(std::string(strrchr(modName, '\\') + 1));
					m_Modules[baseName] = (Utils::StringToLower(modName));
				}
			}
		}
	}

	bool ProcessScanner::IsModuleLoaded(const std::string& moduleName)
	{
		std::string lower = Utils::StringToLower(moduleName);

		HMODULE hMods[1024];
		DWORD cbNeeded;

		if (EnumProcessModulesEx(m_ProcessHandle, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL))
		{
			for (unsigned i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i)
			{
				char modName[MAX_PATH];
				if (GetModuleFileNameExA(m_ProcessHandle, hMods[i], modName, sizeof(modName)))
				{
					std::string baseName = Utils::StringToLower(std::string(strrchr(modName, '\\') + 1));
					if (baseName == lower)
						return true;
				}
			}
		}
		return false;
	}


	uintptr_t ProcessScanner::GetBaseAddress() const
	{
		if (m_BaseAddress)
			return m_BaseAddress;

		HMODULE hMods[1024];
		DWORD cbNeeded;

		if (EnumProcessModules(m_ProcessHandle, hMods, sizeof(hMods), &cbNeeded))
		{
			m_BaseAddress = reinterpret_cast<uintptr_t>(hMods[0]);
			return m_BaseAddress;
		}
		return 0;
	}

	size_t ProcessScanner::GetModuleSize() const
	{
		MODULEINFO modInfo{};
		uintptr_t base = GetBaseAddress();

		if (!base)
			return 0;

		if (GetModuleInformation(m_ProcessHandle, reinterpret_cast<HMODULE>(base), &modInfo, sizeof(modInfo)))
			return modInfo.SizeOfImage;

		return 0;
	}

	bool ProcessScanner::IsAddressValid(uintptr_t address) const
	{
		return address > 0 && address < 0x7FFFFFFFFFFF;
	}

	bool ProcessScanner::IsMemoryReadable(uintptr_t address) const
	{
		MEMORY_BASIC_INFORMATION mbi{};

		if (VirtualQueryEx(m_ProcessHandle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)))
			return (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)));
		return false;
	}

	std::vector<uint8_t> ProcessScanner::ReadMemory(uintptr_t address, size_t size) const
	{
		if (!m_ProcessHandle)
			throw std::runtime_error("Process handle is invalid!");

		std::vector<uint8_t> buffer(size);
		SIZE_T bytesRead = 0;

		if (!ReadProcessMemory(m_ProcessHandle, reinterpret_cast<LPCVOID>(address), buffer.data(), size, &bytesRead))
			throw std::runtime_error(std::format("ReadProcessMemory failed at 0x{:X}", address));
		buffer.resize(bytesRead);
		return buffer;
	}

	std::vector<std::optional<uint8_t>> ProcessScanner::ParsePattern(const std::string& sig)
	{
		std::vector<std::optional<uint8_t>> bytes{};

		const auto nonnull = sig.size() - 1;
		bytes.reserve(nonnull / 2);

		for (size_t i = 0; i < sig.size();)
		{
			if (sig[i] == ' ')
			{
				++i;
				continue;
			}

			if (sig[i] == '?')
			{
				bytes.push_back({});
				i += (i + 1 < sig.size() && sig[i + 1] == '?') ? 2 : 1;
			}
			else
			{
				auto c1 = Utils::CharToHex(sig[i]);
				auto c2 = (i + 1 < sig.size()) ? Utils::CharToHex(sig[i + 1]) : std::nullopt;

				if (c1 && c2)
					bytes.emplace_back(static_cast<uint8_t>((*c1 << 4) + *c2));
				i += 2;
			}
		}

		return bytes;
	}

	uint64_t ProcessScanner::ScanPattern(std::vector<std::optional<uint8_t>> bytes, std::vector<uint8_t> memchunk)
	{
		size_t len = bytes.size();
		size_t current = memchunk.size();
		if (len == 0 || current < len)
			return {};

		size_t max_idx = len - 1;
		size_t max_shift = len;
		size_t wildcard_idx{static_cast<size_t>(-1)};

		for (int i{static_cast<int>(max_idx - 1)}; i >= 0; --i)
		{
			if (!bytes[i])
			{
				max_shift = max_idx - i;
				wildcard_idx = i;
				break;
			}
		}

		if (wildcard_idx == static_cast<size_t>(-1))
			wildcard_idx = 0;

		std::size_t shift_table[UINT8_MAX + 1]{};
		for (std::size_t i{}; i <= UINT8_MAX; ++i)
		{
			shift_table[i] = max_shift;
		}

		for (std::size_t i{wildcard_idx + 1}; i != max_idx; ++i)
		{
			shift_table[*bytes[i]] = max_idx - i;
		}

		const auto scan_end = current - len;
		auto current_idx = 0;
		while (current_idx <= scan_end)
		{
			bool match = true;
			for (size_t i = 0; i < len; ++i)
			{
				auto& b = bytes[i];
				if (b && memchunk[current_idx + i] != *b)
				{
					match = false;
					break;
				}
			}
			if (match)
				return current_idx;

			current_idx++;
		}
		return NULL;
	}

	Pointer ProcessScanner::FindPattern(const std::string& pattern, const std::string& name, size_t chunkSize)
	{
		auto bytes = ParsePattern(pattern);
		auto current_addr = GetBaseAddress();
		auto module_size = GetModuleSize();
		auto end_addr = current_addr + module_size;

		LOG_DEBUG("Scanning memory pattern: '{}'", name);

		while (current_addr < end_addr)
		{
			auto read_size = std::min(chunkSize, end_addr - current_addr);
			std::vector<uint8_t> mem_chunk{};

			try
			{
				mem_chunk = ReadMemory(current_addr, read_size);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("Failed to read memory!: '{}'", e.what());
				current_addr += read_size;
				continue;
			}

			auto offset = ScanPattern(bytes, mem_chunk);
			if (offset != NULL)
			{
				uintptr_t fnd = current_addr + offset;
				LOG_DEBUG("Found pattern '{}' at 0x{:X}", name, fnd);
				return Pointer(m_ProcessHandle, current_addr + offset);
			}

			current_addr += read_size;
		}
		LOG_ERROR("Failed to find pattern {}", name);
		return {};
	}
}
