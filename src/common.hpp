#pragma once

#include <windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>


#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>


namespace YLP
{
    extern HINSTANCE g_Instance;
    extern std::filesystem::path g_ProjectPath;
}

namespace Fonts
{
    extern ImFont* Small;
    extern ImFont* Regular;
    extern ImFont* Bold;
    extern ImFont* Title;

    void Load(ImGuiIO& io);
}

#include "core/logger.hpp"

#define INFO  Logger::eLogLevel::Info
#define WARN  Logger::eLogLevel::Warn
#define ERR   Logger::eLogLevel::Error
#define DEBUG Logger::eLogLevel::Debug

#define LOG_INFO(fmt, ...) Logger::Log(INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Logger::Log(WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Log(ERR, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::Log(DEBUG, fmt, ##__VA_ARGS__)

#include "core/gui/fonts/IconDef.hpp"
#include "core/gui/imgui_helpers.hpp"
