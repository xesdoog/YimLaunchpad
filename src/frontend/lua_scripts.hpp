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

#include <core/github/gitmgr.hpp>
#include <core/gui/renderer.hpp>
#include <core/gui/fonts/fonts.hpp>


namespace YLP::Frontend
{
	class LuaScriptsUI
	{
	public:
		LuaScriptsUI() = default;
		~LuaScriptsUI() {};

		using SortMode = GitHubManager::eSortMode;
		struct SortOption
		{
			const char* label;
			SortMode mode;
		};

		static constexpr SortOption SortOptions[] = {
		    {"Name", SortMode::NAME},
		    {"Stars", SortMode::STARS},
		    {"Date", SortMode::COMMIT},
		    {"Installed", SortMode::INSTALLED},
		};

		static const char* GetSortModeName(const SortMode mode)
		{
			switch (mode)
			{
			case SortMode::NAME: return "Name";
			case SortMode::STARS: return "Stars";
			case SortMode::COMMIT: return "Date";
			case SortMode::INSTALLED: return "Installed";
			default: return "Stars";
			}
		}

		static void DrawRepoCard(const Repository* repo)
		{
			ImGui::BeginChild(std::format("##repo_card_{}", repo->htmlUrl).c_str(), m_CardSize, ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

			std::string date = Utils::FormatDateShort(repo->lastUpdate);
			const char* desc = repo->description.empty() ? "No description." : repo->description.c_str();
			ImGuiStyle& style = ImGui::GetStyle();
			ImVec4 textCol = style.Colors[ImGuiCol_Text];
			const auto titleSize = ImGui::CalcTextSize(repo->name.c_str());

			const bool tileHovered = ImGui::IsMouseHoveringRect(ImGui::GetCursorScreenPos(),
			                             ImVec2(ImGui::GetCursorScreenPos().x + titleSize.x + 5.f,
			                                 ImGui::GetCursorScreenPos().y + titleSize.y + 5.f))
			    && !m_SortPopupOpen;

			const bool titleClicked = tileHovered && ImGui::IsMouseClicked(0);
			const ImVec4 titleCol = tileHovered ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : textCol;

			if (tileHovered)
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			if (titleClicked)
				IO::Open(repo->htmlUrl);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
			ImGui::PushFont(Fonts::Bold);
			ImGui::PushStyleColor(ImGuiCol_Text, titleCol);
			ImGui::SeparatorText(repo->name.c_str());
			ImGui::PopStyleColor();
			ImGui::PopFont();
			ImGui::ToolTip("Open in browser");

			ImGui::PushFont(Fonts::Small);
			ImGui::SetNextWindowBgAlpha(0.f);
			ImGui::BeginChild(std::format("##{}{}", repo->name, desc).c_str(), ImVec2(0, m_CardSize.y - 130), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
			ImGui::TextWrapped(repo->description.c_str());
			if (ImGui::CalcTextSize(repo->description.c_str()).y >= (ImGui::GetContentRegionAvail().y + 20))
				ImGui::ToolTip(repo->description.c_str());
			ImGui::EndChild();

			ImGui::Dummy(ImVec2(0, 10));
			ImGui::Separator();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 4));
			ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 0.7f), ICON_STAR_FILLED);
			ImGui::SameLine();
			ImGui::Text("%d", repo->stars);
			float dateWidth = ImGui::CalcTextSize(date.c_str()).x;
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - dateWidth * 2);
			ImGui::TextDisabled("Updated on: %s", date.c_str());
			ImGui::PopStyleVar(2);

			ImGui::Separator();

			ImGui::BeginDisabled(!std::filesystem::exists(g_YimPath / "scripts") && repo->isDownloading);
			if (!repo->isInstalled)
			{
				if (repo->isDownloading)
					ImGui::Spinner(std::format("##{}", repo->htmlUrl).c_str(), 7.1f);
				else
				{
					if (ImGui::Button(ICON_DOWNLOAD))
						GitHubManager::DownloadRepository(repo->name);
					ImGui::ToolTip("Download");
				}
			}
			else
			{
				if (ImGui::ColoredButton(ICON_DELETE, ImVec4(0.95f, 0.20f, 0.20f, 0.69f), 3.0f))
					GitHubManager::RemoveRepository(repo->name);
				ImGui::ToolTip("Delete");

				ImGui::SameLine();
				if (repo->isDisabled)
				{
					if (ImGui::ColoredButton(ICON_CHECKMARK, ImVec4(0.20f, 0.95f, 0.20f, 0.69f), 2.0f))
					{
						auto src = g_YimPath / "scripts" / "disabled" / repo->name;
						auto dest = g_YimPath / "scripts" / repo->name;
						GitHubManager::MoveRepository(repo->name, src, dest);
					}
					ImGui::ToolTip("Enable");
				}
				else
				{
					if (ImGui::Button(ICON_BLOCK))
					{
						auto src = g_YimPath / "scripts" / repo->name;
						auto dest = g_YimPath / "scripts" / "disabled" / repo->name;
						GitHubManager::MoveRepository(repo->name, src, dest);
					}
					ImGui::ToolTip("Disable");

					if (repo->isPendingUpdate)
					{
						ImGui::SameLine();
						if (ImGui::Button(ICON_UPDATE))
							GitHubManager::DownloadRepository(repo->name);
						ImGui::ToolTip("Update");
					}
				}

				ImGui::SameLine();
				if (ImGui::Button(ICON_FOLDER))
					IO::Open(repo->currentPath.string());
				ImGui::ToolTip("Open folder");
			}
			ImGui::EndDisabled();

			if (repo->isDownloading)
				ImGui::ProgressBar(repo->downloadProgress, ImVec2(m_CardSize.x - 20, 5.f));

			ImGui::PopFont();
			ImGui::EndChild();
		}

		static void Draw()
		{
			auto sortmode = GitHubManager::GetSortMode();
			auto state = GitHubManager::GetState();

			if (ImGui::Button(std::format("{} {}", ICON_SORT, GetSortModeName(sortmode)).c_str(), m_CtrlBtnSize))
				ImGui::OpenPopup("sortscripts");
			ImGui::ToolTip("Sort repositories");

			auto sortButtonPos = ImGui::GetCursorPos();

			ImGui::SameLine(0.f, 10.f);

			if (ImGui::Button(ICON_REFRESH " Refresh", m_CtrlBtnSize))
			{
				ThreadManager::Run([] {
					GitHubManager::FetchRepositories();
				});
			}

			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
			ImGui::TextColored(ImVec4(1.0f, 0.7568, 0.027f, 1.0f), ICON_WARNING);
			ImGui::ToolTip("IMPORTANT: These repositories are for YimMenu V1 only (Legacy).", nullptr, false);

			ImGui::SetNextWindowScroll(ImVec2(m_CtrlBtnSize.x, 0));
			if (ImGui::BeginPopup("sortscripts"))
			{
				m_SortPopupOpen = true;
				ImGui::SetWindowPos(ImVec2(sortButtonPos.x + ImGui::GetStyle().ItemSpacing.x, sortButtonPos.y + (m_CtrlBtnSize.y * 2)));

				for (const auto& opt : SortOptions)
				{
					if (ImGui::MenuItem(opt.label, nullptr, (opt.mode == sortmode)))
					{
						GitHubManager::SetSortMode(opt.mode);
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}
			else
				m_SortPopupOpen = false;

			ImGui::Separator();
			ImGui::Spacing();

			switch (state)
			{
			case GitHubManager::eLoadState::NONE:
				if (ImGui::Button("Load Repositories"))
					GitHubManager::RefreshRepositories();
				break;

			case GitHubManager::eLoadState::LOADING:
			{
				ImVec2 region = ImGui::GetContentRegionAvail();
				float spinnerRadius = region.x * 0.2f;
				float spinnerDiameter = spinnerRadius * 2;
				float spinnerThickness = 4.0f;
				ImVec2 center = ImVec2((region.x - spinnerDiameter) / 2, (region.y - spinnerRadius) / 2);
				ImGui::SetCursorPos(center);
				ImGui::Spinner("##loadrepositories", spinnerRadius, 4.f);
				break;
			}
			case GitHubManager::eLoadState::READY:
			{
				const float padding = 12.f;
				const auto& repos = GitHubManager::GetSortedRepos();
				if (repos.empty())
				{
					ImGui::TextDisabled("No repositories loaded.");
					return;
				}

				const float availX = ImGui::GetContentRegionAvail().x + 10;
				const int cardsPerRow = std::max(1, (int)(availX / (m_CardSize.x + padding)));
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.f);
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 0.3f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
				for (size_t i = 0; i < repos.size(); ++i)
				{
					DrawRepoCard(repos[i]);

					if ((i + 1) % cardsPerRow != 0)
						ImGui::SameLine(0.0f, padding);
				}
				ImGui::PopStyleColor(2);
				ImGui::PopStyleVar(2);
				break;
			}

			case GitHubManager::eLoadState::FAILED:
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load repositories!");
				break;
			}
		}

	private:
		static inline const ImVec2 m_CardSize = ImVec2(300, 190);
		static inline const ImVec2 m_CtrlBtnSize = ImVec2(120, 25);
		static inline bool m_SortPopupOpen = false;
	};
}
