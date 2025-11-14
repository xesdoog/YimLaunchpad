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


namespace YLP
{
	class Logger : Singleton<Logger>
	{
		friend class Singleton<Logger>;

	public:
		Logger() noexcept = default;
		~Logger()
		{
			std::scoped_lock lock(m_Mutex);
			if (m_FileStream.is_open())
			{
				m_FileStream << "\n\n============== Farewell ==============\n";
				m_FileStream.close();
			}
		}

		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;

		enum class eLogLevel
		{
			Info,
			Warn,
			Error,
			Debug
		};

		struct Entry
		{
			std::string timestamp;
			eLogLevel level;
			std::string message;
		};

		static void Init(const std::filesystem::path& filePath, const uintmax_t maxFileSize = 0x7D000)
		{
			GetInstance().InitImpl(filePath, maxFileSize);
		}

		static void InstallExceptionHandler();

		static void SetMaxEntries(size_t count)
		{
			GetInstance().m_MaxEntries = count;
		}

		static void Log(eLogLevel level, std::string_view msg)
		{
			GetInstance().LogImpl(level, std::string(msg));
		}

		template<typename... Args>
		static void Log(eLogLevel level, std::string_view fmt, Args&&... args)
		{
			GetInstance().LogImpl(level, std::vformat(fmt, std::make_format_args(args...)));
		}

		static const std::deque<Entry>& Entries()
		{
			return GetInstance().m_Entries;
		}

		static void Flush()
		{
			GetInstance().FlushImpl();
		}

		static void Clear()
		{
			GetInstance().ClearImpl();
		}

		static void RotateFile()
		{
			GetInstance().RotateFileImpl();
		}

		static const char* ToString(eLogLevel lvl)
		{
			switch (lvl)
			{
			case eLogLevel::Info: return "[INFO]";
			case eLogLevel::Warn: return "[WARN]";
			case eLogLevel::Error: return "[ERROR]";
			case eLogLevel::Debug: return "[DEBUG]";
			default: return "[UNKNOWN]";
			}
		}

	private:
		void InitImpl(const std::filesystem::path& filePath, const uintmax_t maxFileSize);
		void LogImpl(eLogLevel level, std::string_view msg);
		void FlushImpl();
		void ClearImpl();
		void RotateFileImpl();

		mutable std::mutex m_Mutex;
		std::deque<Entry> m_Entries;
		std::filesystem::path m_FilePath;
		std::ofstream m_FileStream;
		std::atomic<bool> m_Initialized{false};
		uintmax_t m_MaxFileSize{0x7D000};
		size_t m_MaxEntries{100};
	};
}
