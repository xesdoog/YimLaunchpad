#pragma once

#include <windows.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>


#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>


namespace YLP
{
	using namespace std::chrono_literals;

	extern HINSTANCE g_Instance;
	extern HWND g_Hwnd;
	extern std::filesystem::path g_ProjectPath;
	extern std::filesystem::path g_YimPath;
	extern std::filesystem::path g_YimV2Path;
	extern std::atomic<bool> g_Running;

	class GlobalMutex
	{
		HANDLE hMutex = nullptr;

	public:
		GlobalMutex(const char* name)
		{
			hMutex = CreateMutexA(nullptr, TRUE, name);
			if (GetLastError() == ERROR_ALREADY_EXISTS)
				hMutex = nullptr;
		}

		bool IsOwned() const
		{
			return hMutex != nullptr;
		}

		~GlobalMutex()
		{
			if (hMutex)
				CloseHandle(hMutex);
		}
	};
}


#include "core/singleton.hpp"
#include "core/logger.hpp"


#define LOG_INFO(fmt, ...) Logger::Log(Logger::eLogLevel::Info, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Logger::Log(Logger::eLogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Log(Logger::eLogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::Log(Logger::eLogLevel::Debug, fmt, ##__VA_ARGS__)                                                                  \


#include "core/utils/utils.hpp"
#include "core/utils/io.hpp"
#include "core/utils/psutils.hpp"
#include "core/settings.hpp"
#include "core/threadmgr.hpp"
#include "core/updater.hpp"
#include "core/gui/imgui_helpers.hpp"


inline decltype(auto) Config()
{
	return YLP::Settings::Get();
}
