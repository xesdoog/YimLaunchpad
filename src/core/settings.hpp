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
	enum eAutoMonitorFlags : uint8_t
	{
		MonitorNone = 0,
		MonitorLegacy = 1 << 0,
		MonitorEnhanced = 1 << 1,
		MonitorBoth = MonitorLegacy | MonitorEnhanced
	};

	using namespace PsUtils;

	class Settings : public Singleton<Settings>
	{
		friend class Singleton<Settings>;

	private:
		Settings() = default;

	public:
		~Settings()
		{
			Save();
		}

		struct Config
		{
			uint8_t autoMonitorFlags = MonitorNone;
			bool debugConsole = false;
			bool supportsV2 = false;
			bool autoInject = false;
			bool autoExit = false;

			int themeIndex = 0;
			int windowWidth = 680;
			int windowHeight = 700;
			int windowX = -1;
			int windowY = -1;
			int mainWindowIndex = 0;

			std::vector<DllInfo> savedDlls{};
		};

		static void Init(const std::filesystem::path& path)
		{
			GetInstance().InitImpl(path);
		}

		static void Destroy()
		{
			GetInstance().SaveImpl();
		}

		Config m_Config;

		static void Save()
		{
			GetInstance().SaveImpl();
		}

		static void Load()
		{
			GetInstance().LoadImpl();
		}

		static Config& Get()
		{
			return GetInstance().m_Config;
		}

		static void Reset()
		{
			GetInstance().ResetImpl();
		}

		template<typename F>
		static void Update(F&& func)
		{
			GetInstance().UpdateImpl(func);
		}

	private:
		std::filesystem::path m_FilePath;

		void InitImpl(const std::filesystem::path& path)
		{
			m_FilePath = path;
			if (!IO::Exists(path))
				Save();
			Load();
		}

		void SaveImpl()
		{
			nlohmann::json j;
			j["auto_monitor_flags"] = m_Config.autoMonitorFlags;
			j["main_window_index"] = m_Config.mainWindowIndex;
			j["debug_console"] = m_Config.debugConsole;
			j["supports_v2"] = m_Config.supportsV2;
			j["auto_inject"] = m_Config.autoInject;
			j["auto_exit"] = m_Config.autoExit;
			j["theme_index"] = m_Config.themeIndex;
			j["window_width"] = m_Config.windowWidth;
			j["window_height"] = m_Config.windowHeight;
			j["window_x"] = m_Config.windowX;
			j["window_y"] = m_Config.windowY;

			for (const auto& dll : m_Config.savedDlls)
				j["saved_dlls"].push_back({
				    {"path", dll.filepath.string()},
				    {"name", dll.name},
				    {"checksum", dll.checksum},
				});

			std::ofstream f(m_FilePath);
			f << std::setw(4) << j;
			f.close();
		}

		void LoadImpl()
		{
			if (!IO::Exists(m_FilePath))
				return;

			std::ifstream f(m_FilePath);
			if (!f.is_open())
				return;

			nlohmann::json j;
			try
			{
				f >> j;
			}
			catch (const std::exception&)
			{
				LOG_WARN("Settings file is invalid, resetting to defaults...");
				ResetImpl();
				return;
			}

			f.close();

			m_Config.autoMonitorFlags = j.value("auto_monitor_flags", MonitorNone);
			m_Config.mainWindowIndex = j.value("main_window_index", 0);
			m_Config.debugConsole = j.value("debug_console", false);
			m_Config.supportsV2 = j.value("supports_v2", false);
			m_Config.autoInject = j.value("auto_inject", false);
			m_Config.autoExit = j.value("auto_exit", false);
			m_Config.themeIndex = j.value("theme_index", 0);
			m_Config.windowX = j.value("window_x", -1);
			m_Config.windowY = j.value("window_y", -1);
			m_Config.windowWidth = j.value("window_width", 680);
			m_Config.windowHeight = j.value("window_height", 720);

			if (j.contains("saved_dlls") && !j["saved_dlls"].is_null() && j["saved_dlls"].is_array())
			{
				for (auto& entry : j["saved_dlls"])
				{
					DllInfo dll{};

					dll.name = entry.value("name", "");
					dll.filepath = entry.value("path", "");
					dll.checksum = entry.value("checksum", "");

					m_Config.savedDlls.push_back(std::move(dll));
				}
			}
		}

		template<typename F>
		void UpdateImpl(F&& func)
		{
			func(m_Config);
			SaveImpl();
		}

		void ResetImpl()
		{
			m_Config = Config{};
			SaveImpl();
		}
	};
}
