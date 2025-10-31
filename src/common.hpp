#pragma once

#include <windows.h>
#include <chrono>
#include <functional>
#include <imgui_internal.h>
#include <iostream>
#include <optional>
#include <string>


namespace YLP
{
    extern HINSTANCE g_Instance;
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

#define LOG_INFO(fmt, ...) Logger::Log(Logger::eLogLevel::Info, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Logger::Log(Logger::eLogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Log(Logger::eLogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::Log(Logger::eLogLevel::Debug, fmt, ##__VA_ARGS__)
#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include "core/gui/fonts/IconDef.hpp"
#include "core/gui/imgui_helpers.hpp"
