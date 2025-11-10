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

#include <core/gui/fonts/fonts.hpp>
#include <core/YimMenu/yimmenu.hpp>
#include <game_icons/gtav.hpp>
#include <game_icons/gtave.hpp>
#include <core/memory/pointers.hpp>
#include <core/memory/procmon.hpp>


namespace YLP::Frontend
{
	class YimMenuUI
	{
	public:
		YimMenuUI() = default;
		~YimMenuUI() {};

		static void Draw()
		{
			if (Config().supportsV2)
			{
				float childWidth = ImGui::GetContentRegionAvail().x;
				float tabButtonWidth = ImGui::CalcTextSize("Enhanced").x + 40.f + ImGui::GetStyle().ItemSpacing.x;
				float centerX = (childWidth - (tabButtonWidth * 2)) / 2;

				ImGui::SetCursorPosX(centerX);
				ImGui::PushFont(Fonts::Title);
				ImGui::RadioButton("Legacy", &Config().mainWindowIndex, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Enhanced", &Config().mainWindowIndex, 1);
				ImGui::PopFont();
			}

			if (Config().mainWindowIndex == 0 || !Config().supportsV2)
				DrawLegacy();

			if (Config().supportsV2 && Config().mainWindowIndex == 1)
				DrawEnhanced();
		}

	private:
		enum eKVflags
		{
			KVflagsNone,
			KVflagsHyperlink,
			KVflagsBullet
		};

		static inline ImFont* GetScaledFont()
		{
			return Renderer::GetWindowSize().x >= 1200 ? Fonts::Regular : Fonts::Small;
		}

		static void LaunchGame(int launcherIndex, eYimVersion version, std::shared_ptr<ProcessMonitor>& monitor)
		{
			m_AttemptedGameLaunch = true;
			std::string cmd;
			switch (launcherIndex)
			{
			case 0:
				cmd = (version == YimMenuV1) ? "steam://run/271590" : "steam://run/3240220";
				break;
			case 1:
			{
				constexpr const wchar_t* REG_SUBKEYS[] = {
				    L"SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V",
				    L"SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V Enhanced"};

				const wchar_t* regSubkey = REG_SUBKEYS[version == YimMenuV2];
				cmd = Utils::WideToUTF8(IO::ReadRegistryKey(HKEY_LOCAL_MACHINE, regSubkey, L"InstallFolder")) + "PlayGTAV.exe";
				break;
			}
			case 2:
				cmd = (version == YimMenuV1) ?
				    "com.epicgames.launcher://apps/9d2d0eb64d5c44529cece33fe2a46482?action=launch&silent=true" :
				    ""; // ?????
				break;
			case 3:
			{
				const std::vector<COMDLG_FILTERSPEC> filters = {{L"EXE (*.exe)", L"*.exe"}};
				cmd = (IO::BrowseFile(filters, L"Select GTA V Executable")).string();
				break;
			}
			default:
				break;
			}

			if (cmd.empty())
			{
				m_AttemptedGameLaunch = false;
				std::string err = "Failed to find an appropriate launch command/path. Please start the game manually.";
				LOG_ERROR(err);
				MsgBox::Error("Error", err.c_str());
				return;
			}

			IO::Open(cmd);
			if (!monitor->WaitForGameReady(35000))
			{
				LOG_WARN("Timed out while waiting for the game to start.");
				m_AttemptedGameLaunch = false;
			}
		}

		static void DrawKeyValue(const char* key,
		    const std::string& value,
		    bool copyable = false,
		    ImVec4 valueColor = ImGui::GetStyle().Colors[ImGuiCol_Text],
		    eKVflags valueDrawFlags = KVflagsNone,
		    std::string optionalUrl = "")
		{
			ImGui::TextUnformatted(key);
			auto valsize = ImGui::CalcTextSize(value.c_str());
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - valsize.x - (copyable ? 40 : 5));
			ImGui::PushStyleColor(ImGuiCol_Text, valueColor);

			switch (valueDrawFlags)
			{
			case KVflagsHyperlink:
				ImGui::TextLinkOpenURL(value.c_str(), optionalUrl.c_str());
				break;
			case KVflagsBullet:
				ImGui::BulletText(value.c_str());
				break;
			default:
				ImGui::TextUnformatted(value.c_str());
				break;
			}

			ImGui::PopStyleColor();
			if (copyable)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton(ICON_COPY))
					ImGui::SetClipboardText(value.c_str());
				ImGui::ToolTip("Copy");
			}
		};

		static void DrawMenuCard(YimMenu& menu,
		    ImTextureID iconTexture,
		    std::shared_ptr<ProcessMonitor>& monitor,
		    GTAPointers pointers)
		{
			bool isRunning = monitor->IsProcessRunning();
			ImVec4 defaultTextCol = ImGui::GetStyle().Colors[ImGuiCol_Text];

			ImGui::Spacing();
			ImGui::BeginChild("##sidebar", ImVec2(240, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
			ImGui::SetCursorPosX(40);
			ImGui::Image(iconTexture, ImVec2(153, 135));
			ImGui::Spacing();

			ImGui::BeginDisabled(m_AttemptedGameLaunch || isRunning);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50);
			ImGui::Combo("##launchers", &m_LauncherIndex, m_Launchers, IM_ARRAYSIZE(m_Launchers));
			ImGui::SameLine();
			if (m_AttemptedGameLaunch)
				ImGui::Spinner("##playbuttonspinenr");
			else if (ImGui::Button(ICON_PLAY))
			{
				ThreadManager::RunDetached([&menu, &monitor] {
					LaunchGame(m_LauncherIndex, menu.m_Version, monitor);
				});
			};
			ImGui::EndDisabled();
			if (!isRunning)
				ImGui::ToolTip("Play");

			ImGui::Spacing();
			ImGui::PushFont(Fonts::Small);
			DrawKeyValue("Status:", isRunning ? ICON_CHECKMARK : ICON_BLOCK, false, isRunning ? ImGreen : ImRed);

			DrawKeyValue("Version:",
			    std::format("{} (Online {})",
			        pointers.GameVersion.empty() ? "?" : pointers.GameVersion,
			        pointers.OnlineVersion.empty() ? "?" : pointers.OnlineVersion));

			auto runtime = (isRunning && g_Pointers.Legacy.GameTime) ? g_Pointers.Legacy.GameTime.Read<int32_t>() : 0;
			DrawKeyValue("Runtime:", Utils::Int32ToTime(runtime / 1000));

			auto baseAddress = monitor->GetBaseAddress();
			DrawKeyValue("Module Base:", std::format("0x{:X}", baseAddress), baseAddress != 0);

			bool heWatchin = monitor->IsHeWatchin();
			DrawKeyValue("BattlEye:", heWatchin ? "Running!" : "Disabled", false, heWatchin ? ImRed : ImGreen);

			ImGui::PopFont();
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 3.0f);
			ImGui::SameLine();

			ImGui::BeginChild("##yimmenu");
			ImGui::PushFont(Fonts::Title);
			ImGui::SeparatorText(menu.m_Name.c_str());
			ImGui::PopFont();
			ImGui::Spacing();

			ImGui::BeginGroup();
			ImGui::PushFont(GetScaledFont());

			DrawKeyValue(
			    "Official GitHub Source: ",
			    std::format("{} {}", ICON_OCTOCAT, menu.m_Name),
			    false,
			    ImGui::GetStyle().Colors[ImGuiCol_Text],
			    KVflagsHyperlink,
			    menu.m_Url);

			bool injected = monitor->IsModuleLoaded(menu.m_DllName);
			std::string shortCommit = menu.m_LastCommitHash.substr(0, std::min<size_t>(7, menu.m_LastCommitHash.size()));

			DrawKeyValue("Short SHA256 Checksum: ", menu.m_Checksum.empty() ? "Unknown" : menu.m_Checksum.substr(0, 16));
			ImGui::ToolTip(std::format("Full SHA: {}", menu.m_Checksum).c_str());

			DrawKeyValue(
			    "Last Commit Hash: ",
			    shortCommit.empty() ? "Unknown" : shortCommit,
			    false,
			    ImGui::GetStyle().Colors[ImGuiCol_Text],
			    menu.m_LastCommitHash.empty() ? KVflagsNone : KVflagsHyperlink,
			    std::format("{}/commit/{}", menu.m_Url, menu.m_LastCommitHash));

			if (!menu.m_LastCommitHash.empty())
			{
				ImGui::ToolTip(std::format("Full commit hash: {}", menu.m_LastCommitHash).c_str(), Fonts::Small, true, 0);
			}

			DrawKeyValue("Injected:",
			    injected ? ICON_CHECKMARK : ICON_BLOCK,
			    false,
			    injected ? ImGreen : ImGui::GetStyle().Colors[ImGuiCol_Text]);

			ImGui::PopFont();
			ImGui::EndGroup();

			ImGui::Separator();
			ImGui::Spacing();

			// this spaghetti got tangeled rq...
			if (menu.m_IsDownloading)
				ImGui::ProgressBar(menu.m_DownloadProgress, ButtonBig);
			else
			{
				if (!menu)
				{
					if (ImGui::Button(std::format("{} {}", ICON_DOWNLOAD, "Download").c_str(), ButtonBig))
					{
						ThreadManager::Run([&menu] {
							menu.Download();
						});
					}
				}
				else
				{
					if (menu.m_PendingUpdate)
					{
						if (ImGui::Button(ICON_UPDATE))
						{
							ThreadManager::Run([&menu] {
								menu.Download();
							});
						}
						ImGui::ToolTip("Update");
						ImGui::SameLine();
						ImGui::Text("Update Available!");
					}
					else
					{
						if (menu.m_State == Busy)
							ImGui::Spinner("Please wait...");
						else
						{
							if (ImGui::Button(ICON_REFRESH))
							{
								ThreadManager::Run([&menu] {
									menu.CheckForUpdates();
								});
							}
							ImGui::SameLine();
							ImGui::Text("Check For Updates");
						}
					}

					ImGui::Spacing();
					bool disabledCond = (menu.m_Version == YimMenuV1 && Config().autoInject) || menu.m_IsInjected || !monitor->IsProcessRunning();
					ImGui::BeginDisabled(disabledCond);
					if (ImGui::Button(std::format("{} Inject", ICON_INJECT).c_str(), ButtonBig))
					{
						ThreadManager::Run([&menu] {
							auto result = menu.Inject();
							if (!result.success)
							{
								LOG_ERROR("{}", result.message);
								MsgBox::Error("Error", result.message.c_str());
							}
						});
					}
					ImGui::EndDisabled();
					if (disabledCond)
						ImGui::ToolTip("Currently unavailable.\n\nMake sure the game is running, auto-inject is off, and the menu isn't already injected.");
				}
			}
			ImGui::EndChild();
		}

		static void DrawLegacy()
		{
			if (!IconGTAV) // this is redundant because renderer loads once anyway but it would bebetter to make it lazy-load instead. i'm too lazy to do it
				IconGTAV = Renderer::LoadTextureFromMemory(gta_legacy_data, gta_legacy_size, "IconGTAV");
			DrawMenuCard(g_YimV1, IconGTAV, g_ProcLegacy, g_Pointers.Legacy);
		}

		static void DrawEnhanced()
		{
			if (!IconGTAVE)
				IconGTAVE = Renderer::LoadTextureFromMemory(gta_enhanced_data, gta_enhanced_size, "IconGTAVE");

			DrawMenuCard(g_YimV2, IconGTAVE, g_ProcEnhanced, g_Pointers.Enhanced);
		}

	private:
		static inline ImTextureID IconGTAV = NULL;
		static inline ImTextureID IconGTAVE = NULL;
		static inline ImVec4 ImRed = ImVec4(1, 0, 0, 1);
		static inline ImVec4 ImGreen = ImVec4(0, 1, 0, 1);
		static inline ImVec4 ImBlue = ImVec4(0, 0, 1, 1);
		static inline ImVec2 ButtonBig = ImVec2(150, 37);
		static inline int m_LauncherIndex;
		static inline bool m_AttemptedGameLaunch = false;

		static inline const char* m_Launchers[] = {
		    "Steam",
		    "Rockstar Games Launcher",
		    "Epic Games Launcher",
		    "Manual",
		};
	};
}
