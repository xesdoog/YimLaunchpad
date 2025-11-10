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


#include <exception>
#include <cstdio>
#include <ctime>

#include "logger.hpp"


namespace YLP
{
	void Logger::InstallExceptionHandler()
	{
		std::set_terminate([]() {
			try
			{
				if (auto p = std::current_exception())
					std::rethrow_exception(p);
				else
					Log(eLogLevel::Error, "Terminated due to unknown exception!");
			}
			catch (const std::exception& e)
			{
				try
				{
					Log(eLogLevel::Error, "Unhandled exception: %s", e.what());
				}
				catch (...)
				{
					std::fprintf(stderr, "Unhandled exception (logger unavailable): %s\n", e.what());
				}
			}
			catch (...)
			{
				try
				{
					Log(eLogLevel::Error, "Unhandled non-standard exception!");
				}
				catch (...)
				{
					std::fprintf(stderr, "Unhandled non-standard exception (logger unavailable)!\n");
				}
			}
			std::abort();
		});
	}

	void Logger::InitImpl(const std::filesystem::path& file, const uintmax_t maxFileSize)
	{
		if (m_Initialized)
			return;

		std::scoped_lock lock(m_Mutex);

		if (m_FileStream.is_open())
			m_FileStream.close();

		m_FileStream.open(file, std::ios::out | std::ios::app);
		if (!m_FileStream.is_open())
		{
			std::fprintf(stderr, "Logger failed to open file: %s\n", file.string().c_str());
			return;
		}

		InstallExceptionHandler();
		m_FilePath = file;
		m_MaxFileSize = maxFileSize;
		m_Initialized = true;
	}

	void Logger::LogImpl(eLogLevel lvl, std::string_view msg)
	{
		if (!m_Initialized)
			return;

		const std::string ts = Utils::FormatTime();
		const Entry entry{ts, lvl, std::string(msg)};

		std::scoped_lock lock(m_Mutex);

		if (m_Entries.size() >= m_MaxEntries)
			m_Entries.pop_front();

		m_Entries.push_back(entry);
		RotateFile();

		if (m_FileStream.is_open())
		{
			char line[1536];
			std::snprintf(line, sizeof(line), "[%s] %s: %s\n", ts.c_str(), ToString(lvl), msg.data());
			m_FileStream << line;
			m_FileStream.flush();
		}
	}

	void Logger::FlushImpl()
	{
		std::scoped_lock lock(m_Mutex);
		if (m_FileStream.is_open())
			m_FileStream.flush();
	}

	void Logger::ClearImpl()
	{
		std::scoped_lock lock(m_Mutex);
		m_Entries.clear();
	}

	void Logger::RotateFileImpl()
	{
		if (!std::filesystem::exists(m_FilePath))
			return;

		auto fileSize = std::filesystem::file_size(m_FilePath);
		if (fileSize < m_MaxFileSize)
			return;

		if (m_FileStream.is_open())
			m_FileStream.close();

		auto now = std::chrono::system_clock::now();
		std::time_t t = std::chrono::system_clock::to_time_t(now);
		std::tm tm{};
		localtime_s(&tm, &t);

		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
		std::string timestamp = oss.str();

		auto rotatedDir = g_ProjectPath / "Logs";
		std::filesystem::create_directories(rotatedDir);

		auto rotatedFile = rotatedDir / (m_FilePath.filename().string() + "_" + timestamp + ".bak");

		try
		{
			std::filesystem::rename(m_FilePath, rotatedFile);
		}
		catch (const std::exception& e)
		{
			std::fprintf(stderr, "[Logger] Failed to rotate log: %s\n", e.what());
		}

		m_FileStream.open(m_FilePath, std::ios::out | std::ios::trunc);
		if (!m_FileStream.is_open())
		{
			std::fprintf(stderr, "[Logger] Failed to reopen log after rotation!\n");
			return;
		}

		std::vector<std::filesystem::path> backups;
		for (auto& f : std::filesystem::directory_iterator(rotatedDir))
		{
			if (f.path().extension() == ".bak")
				backups.push_back(f.path());
		}

		if (backups.size() > 5)
		{
			std::sort(backups.begin(), backups.end(), [](auto& a, auto& b) {
				return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
			});

			for (size_t i = 5; i < backups.size(); ++i)
				std::filesystem::remove(backups[i]);
		}
	}
}
