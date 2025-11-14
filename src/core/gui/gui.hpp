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

#include "renderer.hpp"
#include "fonts/fonts.hpp"
#include <frontend/main_window.hpp>
#include <frontend/lua_scripts.hpp>
#include <frontend/injector.hpp>


namespace YLP
{
	using GuiCallBack = std::function<void()>;

	class GUI : public Singleton<GUI>
	{
		friend class Singleton<GUI>;

	private:
		GUI() = default;
		~GUI() noexcept = default;

	public:
		static void Init()
		{
			GetInstance().InitImpl();
		}

		static bool AddTab(const std::string_view& name, GuiCallBack&& callback, std::optional<std::string_view> hint)
		{
			return GetInstance().AddTabImpl(name, std::move(callback), hint);
		}

		static void Draw()
		{
			GetInstance().DrawImpl();
		}

		static void DrawTabBar()
		{
			GetInstance().DrawTabBarImpl();
		}

		static void DrawSettings()
		{
			GetInstance().DrawSettingsImpl();
		}

		static void DrawDebugConsole()
		{
			GetInstance().DrawDebugConsoleImpl();
		}

		static void ToggleDisableUI(bool toggle) noexcept
		{
			GetInstance().m_ShouldDisableUI = toggle;
		}

		static void SetActiveTab(const std::string_view& name)
		{
			GetInstance().SetActiveTabImpl(name);
		}

		static void DrawAboutSection();
		static void SetupStyle();

	private:
		void InitImpl();
		void DrawImpl();
		void DrawTabBarImpl();
		void DrawDebugConsoleImpl();
		void DrawSettingsImpl();
		void SetActiveTabImpl(const std::string_view& name);
		bool AddTabImpl(const std::string_view& name, GuiCallBack&& callback, std::optional<std::string_view> hint);

		struct Tab
		{
			std::string_view m_Name;
			GuiCallBack m_Callback;
			std::optional<std::string_view> m_Hint;
		};

		int m_TabIndex = 0;
		bool m_ShouldDisableUI = false;

		Tab* m_ActiveTab = nullptr;
		ImVec2 m_WindowSize;

		std::vector<std::shared_ptr<Tab>> m_Tabs;
	};
}
