#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>


namespace YLP
{
    class Logger final 
   {
    private:
        Logger() {};

    public:
        ~Logger()
        {
            Log(eLogLevel::Info, "============== Farewell ==============");

            if (m_File.is_open())
                m_File.close();
        };

        Logger(const Logger&) = delete;
        Logger(Logger&&) noexcept = delete;
        Logger& operator=(const Logger&) = delete;
        Logger& operator=(Logger&&) noexcept = delete;

        enum class eLogLevel
        {
            Info,
            Warn,
            Error,
            Debug
        };

        struct LogEntry
        {
            std::string timestamp;
            eLogLevel level;
            std::string message;
        };

        static void Init(const std::filesystem::path& file_path)
        {
            GetInstance().InitImpl(file_path);
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

        static const std::vector<LogEntry>& Entries()
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

        static const char* ToString(eLogLevel level)
        {
            switch (level) {
            case eLogLevel::Info:  return "[INFO]";
            case eLogLevel::Warn:  return "[WARN]";
            case eLogLevel::Error: return "[ERROR]";
            case eLogLevel::Debug: return "[DEBUG]";
            default: return "[UNKNOWN]";
            }
        }

    private:
        void InitImpl(const std::filesystem::path& file_path)
        {
            if (m_Initialized)
                return;

            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_File.is_open())
                m_File.close();

            m_File.open(file_path, std::ios::out | std::ios::app);
            if (!m_File.is_open())
            {
                std::fprintf(stderr, "Logger failed to open file: %s\n", file_path.string().c_str());
                return;
            }

            std::set_terminate([]()
            {
                try
                {
                    if (auto pExc = std::current_exception())
                        std::rethrow_exception(pExc);
                    else
                        Logger::Log(eLogLevel::Error, "Terminated due to unknown exception!");
                }
                catch (const std::exception& e)
                {
                    try {
                        Logger::Log(eLogLevel::Error, "Unhandled exception: {}", e.what());
                    }
                    catch (...) {
                        std::fprintf(stderr, "Unhandled exception (logger unavailable): %s\n", e.what());
                    }
                }
                catch (...)
                {
                    try {
                        Logger::Log(eLogLevel::Error, "Unhandled non-standard exception!");
                    }
                    catch (...) {
                        std::fprintf(stderr, "Unhandled non-standard exception (logger unavailable)!\n");
                    }
                }
                std::abort();
            });

            m_Initialized = true;
        }

        void LogImpl(eLogLevel level, const std::string& msg)
        {
            const std::string timestamp = CurrentTime();
            const LogEntry entry{ timestamp, level, msg };

            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Entries.push_back(entry);

                if (m_File.is_open())
                {
                    m_File << std::format("[{}] {}: {}\n", timestamp, ToString(level), msg);
                    m_File.flush();
                }
            }
        }

        void FlushImpl()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_File.is_open())
                m_File.flush();
        }

        void ClearImpl()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Entries.clear();
        }

        static std::string CurrentTime()
        {
            auto now = std::chrono::system_clock::now();
            std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &time);

            std::ostringstream oss;
            oss << std::put_time(&tm, "%H:%M:%S");
            return oss.str();
        }

    private:
        mutable std::mutex m_Mutex;
        std::atomic<bool> m_Initialized;
        std::vector<LogEntry> m_Entries;
        std::ofstream m_File;

        static Logger& GetInstance()
        {
            static Logger i{};
            return i;
        }
    };
}
