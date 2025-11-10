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


#include "gui.hpp"
#include <core/memory/pointers.hpp>


namespace YLP
{
	using namespace Frontend;

	void GUI::InitImpl()
	{
		Fonts::Load(ImGui::GetIO());
		SetupStyle();

		AddTab(ICON_YIM, &YimMenuUI::Draw, "YimMenu");
		AddTab(ICON_INJECT, &InjectorUI::Draw, "Injector");
		AddTab(ICON_LUA, &LuaScriptsUI::Draw, "Lua Scripts");
		AddTab(ICON_SETTINGS, DrawSettings, "Settings");
		AddTab(ICON_HELP, DrawAboutSection, "About");

		m_ActiveTab = m_Tabs.front().get();
	}

	bool GUI::AddTabImpl(const char* name, GuiCallBack&& callback, std::optional<const char*> hint)
	{
		for (const auto& tab : m_Tabs)
		{
			if (tab->m_Name == name)
				return false;
		}

		m_Tabs.push_back(std::make_shared<Tab>(Tab{name, std::move(callback), hint}));
		return true;
	}

	void GUI::DrawImpl()
	{
		m_WindowSize = Renderer::GetWindowSize();
		ImGui::SetNextWindowSize(m_WindowSize, ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.5);
		ImGui::Begin(
		    "YLP",
		    nullptr,
		    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

		ImGui::PushFont(Fonts::Regular);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(10.f, 10.f));

		ImGui::BeginDisabled(m_ShouldDisableUI);
		DrawTabBar();

		const float lowerChildHeight = std::min(m_WindowSize.y * 0.4f, 280.0f);
		float cbChildHeight = Config().debugConsole ? m_WindowSize.y - lowerChildHeight : 0;
		ImGui::SetNextWindowBgAlpha(0.f);
		if (ImGui::BeginChild("##main", ImVec2(0, cbChildHeight), ImGuiChildFlags_Border))
		{
			ImGui::Dummy(ImVec2(1, 5));
			if (m_ActiveTab && m_ActiveTab->m_Callback)
				m_ActiveTab->m_Callback();
		}
		ImGui::EndChild();

		DrawDebugConsole();

		ImGui::EndDisabled();
		ImGui::PopFont();
		ImGui::PopStyleVar(2);
		ImGui::End();
	}

	void GUI::DrawTabBarImpl()
	{
		int tabCount = static_cast<int>(m_Tabs.size());
		if (tabCount == 0)
			return;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float availWidth = ImGui::GetContentRegionAvail().x;
		const float iconSize = 40.0f;
		const float tabHeight = 51.0f;
		const float totalWidth = tabCount * iconSize + (tabCount - 1) * spacing;
		const float startX = (availWidth - totalWidth) * 0.5f;
		static float underlineX = 0.0f;
		static float underlineW = 0.0f;
		static float underlineTargetX = 0.0f;
		static float underlineTargetW = 0.0f;

		ImVec2 basePos = ImGui::GetCursorScreenPos();
		float yCenter = basePos.y + tabHeight * 0.5f - iconSize * 0.5f;

		ImGui::PushFont(Fonts::Title);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);

		for (int i = 0; i < tabCount; i++)
		{
			auto& tab = *m_Tabs[i];
			bool selected = (m_ActiveTab == &tab);
			ImVec2 iconPos = ImVec2(ImGui::GetCursorScreenPos().x, yCenter);
			ImRect rect(iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));

			ImGui::PushID(i);
			ImGui::InvisibleButton(tab.m_Name, rect.GetSize());
			bool hovered = ImGui::IsItemHovered();
			bool clicked = ImGui::IsItemClicked();
			bool held = hovered && ImGui::IsMouseDown(0);
			ImVec2 rectMod = ImVec2(held ? 1 : 0, held ? 1 : 0);

			if (hovered || selected)
			{
				ImU32 col1 = selected ? IM_COL32(70, 130, 255, held ? 180 : 230) : IM_COL32(60, 60, 80, held ? 100 : 150);
				ImU32 col2 = selected ? IM_COL32(100, 160, 255, held ? 200 : 255) : IM_COL32(80, 80, 110, held ? 130 : 180);
				drawList->AddRectFilledMultiColor(
				    rect.Min + rectMod,
				    rect.Max - rectMod,
				    col1,
				    col2,
				    col2,
				    col1);
			}

			ImVec2 textSize = ImGui::CalcTextSize(tab.m_Name);
			ImVec2 textPos = ImVec2(
			    rect.Min.x + (iconSize - textSize.x) * 0.5f,
			    rect.Min.y + (iconSize - textSize.y) * 0.5f);

			drawList->AddText(textPos, selected ? IM_COL32_BLACK : IM_COL32_WHITE, tab.m_Name);

			if (hovered && tab.m_Hint.has_value())
				ImGui::ToolTip(tab.m_Hint.value(), Fonts::Regular);

			if (clicked)
			{
				m_ActiveTab = &tab;
				underlineTargetX = rect.Min.x;
				underlineTargetW = iconSize;
			}

			if (selected)
			{
				underlineTargetX = rect.Min.x;
				underlineTargetW = iconSize;
			}

			ImGui::PopID();

			if (i < tabCount - 1)
				ImGui::SameLine(0, spacing);
		}
		ImGui::PopFont();

		float speed = ImGui::GetIO().DeltaTime * 12.0f;
		underlineX = ImLerp(underlineX, underlineTargetX, speed);
		underlineW = ImLerp(underlineW, underlineTargetW, speed);

		float underlineHeight = 3.0f;
		ImVec2 underlinePos(underlineX, basePos.y + tabHeight - underlineHeight);
		ImVec2 underlineEnd(underlineX + underlineW, underlinePos.y + underlineHeight);

		drawList->AddRectFilled(underlinePos, underlineEnd, IM_COL32(100, 180, 255, 255), 1.5f);
	}

	void GUI::DrawDebugConsoleImpl()
	{
		if (!Config().debugConsole)
			return;

		ImGui::Spacing();
		if (ImGui::BeginChild("##console", ImVec2(0, 0), ImGuiChildFlags_Border))
		{
			auto& logEntries = Logger::Entries();

			ImGui::BulletText("Output");
			ImGui::SameLine(0, 20);
			ImGui::PushFont(Fonts::Small);

			ImGui::BeginDisabled(logEntries.empty());
			if (ImGui::Button(ICON_COPY))
			{
				std::string text;
				for (const auto& e : Logger::Entries())
					text += "[" + e.timestamp + "] " + Logger::ToString(e.level) + " " + e.message + "\n";
				ImGui::SetClipboardText(text.c_str());
			}
			ImGui::ToolTip("Copy all log entries");

			ImGui::SameLine();
			if (ImGui::Button(ICON_DELETE))
				Logger::Clear();
			ImGui::ToolTip("Clear all log entries");
			ImGui::EndDisabled();

			ImGui::SetNextWindowBgAlpha(1.f);
			if (ImGui::BeginChild("##debug_output", ImVec2(0, 0), true))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
				ImGui::PushTextWrapPos(0.0f);

				for (int i = 0; i < logEntries.size(); ++i)
				{
					auto& entry = logEntries[i];
					ImVec4 color;

					switch (entry.level)
					{
					case Logger::eLogLevel::Info: color = ImVec4(0.7f, 0.7f, 0.7f, 1.f); break;
					case Logger::eLogLevel::Warn: color = ImVec4(1.f, 0.8f, 0.f, 1.f); break;
					case Logger::eLogLevel::Error: color = ImVec4(1.f, 0.3f, 0.3f, 1.f); break;
					case Logger::eLogLevel::Debug: color = ImVec4(0.5f, 0.8f, 1.f, 1.f); break;
					default: color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
					}

					auto text = std::format("[{}] {}: {}\n", entry.timestamp, Logger::ToString(entry.level), entry.message);
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::PushID(i);
					ImGui::WrappedSelectable(text.c_str());
					ImGui::PopID();
					ImGui::PopStyleColor();
					ImGui::Spacing();

					ImGui::ToolTip("Left click to copy");
					if (ImGui::IsItemHovered() && ImGui::IsItemClicked(0))
						ImGui::SetClipboardText(text.c_str());
				}

				ImGui::PopTextWrapPos();
				ImGui::PopStyleVar();

				if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
					ImGui::SetScrollHereY(1.0f);
			}
			ImGui::EndChild();
			ImGui::PopFont();
		}
		ImGui::EndChild();
	}

	const char* style_names[] = {"Dark", "Light", "Classic"};
	const char* autoModes[] = {"None", "Legacy Only", "Enhanced Only", "Legacy & Enhanced"};

	static void SetTheme(int index)
	{
		switch (index)
		{
		case 0: ImGui::StyleColorsDark(); break;
		case 1: ImGui::StyleColorsLight(); break;
		case 2: ImGui::StyleColorsClassic(); break;
		default: ImGui::StyleColorsClassic();
		}
	}

	static void DrawThemes()
	{
		if (ImGui::BeginCombo("##styleSelector", style_names[Config().themeIndex]))
		{
			for (int i = 0; i < IM_ARRAYSIZE(style_names); i++)
			{
				if (ImGui::Selectable(style_names[i], Config().themeIndex == i))
				{
					Config().themeIndex = i;
					SetTheme(i);
				}
				else if (Config().themeIndex == i)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	static void SwitchMonitorMode(uint8_t mode)
	{
		switch (mode)
		{
		case MonitorNone:
			g_ProcLegacy->Stop();
			g_ProcEnhanced->Stop();
			Config().autoInject = false;
			break;
		case MonitorLegacy:
			g_ProcLegacy->Start(1ms);
			g_ProcEnhanced->Stop();
			break;
		case MonitorEnhanced:
			g_ProcLegacy->Stop();
			g_ProcEnhanced->Start(1ms);
			break;
		case MonitorBoth:
			g_ProcLegacy->Start(1ms);
			g_ProcEnhanced->Start(100ms);
			break;
		default:
			break;
		}
	}

	static void DrawAutoModes()
	{
		if (ImGui::BeginCombo("##YLPAuto", autoModes[Config().autoMonitorFlags]))
		{
			for (int i = 0; i < IM_ARRAYSIZE(autoModes); i++)
			{
				if (ImGui::Selectable(autoModes[i], Config().autoMonitorFlags == i))
				{
					Config().autoMonitorFlags = i;
					SwitchMonitorMode(i);
				}
				else if (Config().autoMonitorFlags == i)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	void GUI::DrawSettingsImpl()
	{
		auto version = YLPUpdater.GetLocalVersion();
		ImGui::PushFont(Fonts::Small);
		ImGui::Text("YLP v%d.%d.%d.%d", version.major, version.build, version.minor, version.patch);
		ImGui::PopFont();
		ImGui::Separator();

		ImGui::TitleText("General", true);
		auto updateState = YLPUpdater.GetState();
		ImGui::BeginDisabled(updateState == Updater::UpdateState::Error);
		switch (updateState)
		{
		case Updater::UpdateState::Idle:
		{
			if (ImGui::Button(ICON_REFRESH))
				YLPUpdater.Check();
			ImGui::SameLine();
			ImGui::Text("Check For Updates");
			break;
		}
		case Updater::UpdateState::Checking:
			ImGui::Spinner("Please Wait...");
			break;
		case Updater::UpdateState::Pending:
		{
			if (ImGui::Button(ICON_DOWNLOAD))
				YLPUpdater.Download();
			ImGui::SameLine();
			ImGui::Text("A new version of YLP is out");
			break;
		}
		case Updater::UpdateState::Downloading:
		{
			ImGui::ProgressBar(YLPUpdater.GetProgress(), ImVec2(160, 25));
			ImGui::SameLine();
			ImGui::Text("Downloading...");
			break;
		}
		}
		ImGui::EndDisabled();

		ImGui::Checkbox("Output Console", &Config().debugConsole);
		ImGui::HelpMarker("Toggle the output console at the bottom of the UI.");

		ImGui::Checkbox("V2 Support [WIP]", &Config().supportsV2);
		ImGui::HelpMarker("Enable partial support for YimMenu V2 (work in progress)");

		ImGui::Spacing();
		ImGui::TitleText("Automatic Mode", true);
		ImGui::PushFont(Fonts::Small);
		ImGui::Bullet();
		ImGui::SameLine();
		ImGui::TextWrapped("Continuously monitor both versions of GTA V processes, auto-inject, detect injection crashes, etc.");
		ImGui::PopFont();
		DrawAutoModes();

		ImGui::BeginDisabled(Config().autoMonitorFlags & MonitorNone);
		ImGui::Checkbox("Auto-Inject", &Config().autoInject);
		ImGui::EndDisabled();
		ImGui::HelpMarker("V2 is currently not supported.");

		ImGui::SameLine();
		ImGui::Checkbox("Auto-Exit", &Config().autoExit);
		ImGui::HelpMarker("Automatically exit after injecting a dll **(Only for auto-inject)**.");

		ImGui::Spacing();
		ImGui::TitleText("Themes", true);
		DrawThemes();
	}

	void GUI::SetupStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		ImGui::StyleColorsClassic();

		style.WindowRounding = 0.0f;
		style.FrameRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.ChildRounding = 5.0f;
		style.TabRounding = 4.0f;
		style.FrameBorderSize = 1.0f;
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.GrabRounding = 3.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.3f;
		style.PopupBorderSize = 1.0f;
		style.HoverDelayNormal = 1.2f;
		style.HoverDelayShort = 0.3f;

		SetTheme(Config().themeIndex);

		/*
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
        colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
        colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = colors[ImGuiCol_Tab];
        colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabActive];
        colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
        colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.93f, 1.0f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.45f, 0.95f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.55f, 1.00f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.45f, 0.95f, 1.0f);
        colors[ImGuiCol_ChildBg].w = 1.0f;
        colors[ImGuiCol_WindowBg].w = 1.0f;
        */
	}

	static void DrawHeaderAndText(const char* header, const char* text, std::initializer_list<const char*> bullets = {})
	{
		ImGui::PushFont(Fonts::Title);
		ImGui::Text(header);
		ImGui::PopFont();
		ImGui::Separator();
		ImGui::TextWrapped(text);
		if (bullets.size() > 0)
		{
			for (auto& c : bullets)
			{
				ImGui::Bullet();
				ImGui::TextWrapped(c);
			}
		}
		ImGui::Spacing();
	}

	void GUI::DrawAboutSection()
	{
		DrawHeaderAndText("About YLP",
		    R"(
YLP (formerly YimLaunchpad) is a free and open-source companion application created by a member of the community, for the community.
It is not affiliated, associated, or endorsed by any mod menu or third-party commercial tool.

YLP exists solely to enhance the player's experience through transparency and convenience.
)");

		DrawHeaderAndText("Philosophy",
		    R"(
YLP was built on three simple principles:
)",
		    {"Transparency: all source code is open and available for review.",
		        "Safety: nothing is hidden, obfuscated, or phoning home. What you see is what you get.",
		        "Respect: YLP never modifies, redistributes, or monetizes features or third-party content."});

		ImGui::InfoCallout(ImGui::ImCalloutType::Important,
		    R"(
YLP is provided "as is", without any warranty of any kind, express or implied.
The author shall not be held liable for any damages, data loss, or issues arising from the use or misuse of this software.
)");
		ImGui::InfoCallout(ImGui::ImCalloutType::Warning,
		    R"(
Please be cautious when downloading tools or mods from the internet.
Only download from official repositories or verified community sources.
Open-source projects like YLP allow you to inspect the code yourself; a core value that protects both your system and your data.
)");
		DrawHeaderAndText("Licenses and Acknowledgements",
		    R"(
YLP makes use of several open-source libraries and assets that are licensed under their respective terms.
Full license texts and credits are available in the Third-Party Licenses document in the source repository.

We extend our gratitude to the open-source community for their incredible contributions.
)");

		////////////////////////////////////////////////////////

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::Spacing();
		// ImGui::PushFont(Fonts::Small);
		ImGui::TextLinkOpenURL(std::format("{} YLP", ICON_OCTOCAT).c_str(), "https://github.com/xesdoog/YLP");
		ImGui::TextLinkOpenURL("License", "https://www.gnu.org/licenses/gpl-3.0.en.html");
		ImGui::TextLinkOpenURL("3rd Party", "https://github.com/xesdoog/YLP/blob/main/Thirdparty/Readme.md");
		ImGui::TextLinkOpenURL("IcoMoon.io", "https://https://icomoon.io/");
		ImGui::TextLinkOpenURL("JetBrainsMono", "https://jetbrains.com/lp/mono/");
		// ImGui::PopFont();
	}
}
