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
#include <core/gui/msgbox.hpp>


namespace YLP::Frontend
{
	using namespace PsUtils;

	class InjectorUI
	{
	public:
		InjectorUI() = default;
		~InjectorUI() noexcept = default;

		static void Draw()
		{
			auto processes = processList.GetSnapshot();
			ImVec2 windowSize = ImGui::GetWindowSize();
			float regioinWidth = windowSize.x / 1.4f;
			float regionHeight = windowSize.y / 1.4f;
			float centerX = (ImGui::GetContentRegionAvail().x - regioinWidth) / 2;
			float pidTextWidth = ImGui::CalcTextSize("[01234567890]").x;

			ImGui::SetCursorPosX(centerX);
			ImGui::SetNextItemWidth(regioinWidth);
			if (ImGui::BeginCombo("##processList",
			        std::format("{} {}",
			            ICON_PROCESS,
			            selectedProcess.m_Name.empty() ? "Process List" : selectedProcess.m_Name)
			            .c_str()))
			{
				if (!initialized)
				{
					processList.StartUpdating();
					sortedProcessList = processes;
					initialized = true;
				}

				ImGui::SetNextItemWidth(regioinWidth - 30);
				ImGui::InputTextWithHint("##SearchBox", ICON_SEARCH, searchBuffer, IM_ARRAYSIZE(searchBuffer));

				{
					std::scoped_lock m_Mutex;
					sortedProcessList.clear();
					for (auto& p : processes)
					{
						if (searchBuffer[0] == '\0' || Utils::StringToLower(std::string(p.m_Name)).find(Utils::StringToLower(searchBuffer)) != std::string::npos)
							sortedProcessList.push_back(p);
					}
				}

				ImGui::Columns(2, nullptr, false);
				ImGui::SetColumnWidth(0, regioinWidth - pidTextWidth);
				ImGuiListClipper Clipper;
				Clipper.Begin(sortedProcessList.size());
				while (Clipper.Step())
				{
					for (int i = Clipper.DisplayStart; i < Clipper.DisplayEnd; ++i)
					{
						auto p = sortedProcessList[i];
						if (p.m_Name.empty())
							continue;

						ImGui::PushID(p.m_Pid);
						if (ImGui::Selectable(std::format("{}", p.m_Name).c_str(),
						        p.m_Pid == selectedProcess.m_Pid,
						        0,
						        ImVec2(regioinWidth - 30, 0.f)))
						{
							selectedProcess = p;
							ImGui::CloseCurrentPopup();
						}
						ImGui::PopID();
						ImGui::NextColumn();
						ImGui::Text("[%u]", p.m_Pid);
						ImGui::NextColumn();
					}
				}
				ImGui::Columns(1);
				ImGui::EndCombo();
			}
			else if (initialized)
			{
				processList.StopUpdating();
				initialized = false;
			}

			ImGui::Dummy(ImVec2(0, 10));
			ImGui::SetCursorPosX(centerX);
			ImGui::BeginChild("##injectorstuff",
			    ImVec2(regioinWidth, ImGui::GetContentRegionAvail().y - 50.f),
			    ImGuiChildFlags_Border,
			    ImGuiWindowFlags_NoScrollbar);

			{
				ImGui::SeparatorText(std::format("{} DLL List", ICON_COGS).c_str());
				ImGui::Spacing();
				for (auto& dll : Config().savedDlls)
				{
					if (!dll.filepath.empty())
					{
						if (ImGui::Selectable(std::format("{} [{}]",
						                          dll.filepath.filename().string(),
						                          dll.checksum.substr(0, 8))
						                          .c_str(),
						        selectedDLL.filepath == dll.filepath))
							selectedDLL = dll;
					}
				}
			}
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("##buttons", ImVec2(0, ImGui::GetContentRegionAvail().y - 50.f), ImGuiChildFlags_None);
			ImGui::Dummy(ImVec2(0, 40));
			ImGui::PushFont(Fonts::Title);
			if (ImGui::ColoredButton(ICON_ADD, ImVec4(0.20f, 0.95f, 0.20f, 0.69f), 2.0f))
			{
				ThreadManager::RunDetached([] {
					DllInfo newdll = PsUtils::AddDLL();
					if (newdll.filepath.empty()) // canceled??
						return;

					for (auto& dll : Config().savedDlls)
					{
						if (dll.filepath == newdll.filepath)
						{
							LOG_WARN("File already exists!");
							newdll.ok = false;
							return;
						}
					}

					if (newdll.ok)
						Config().savedDlls.push_back(newdll);
					else
						LOG_ERROR("Failed to add DLL file: {}", newdll.error);
				});
			}
			ImGui::ToolTip("Add a new DLL");

			ImGui::BeginDisabled(selectedDLL.filepath.empty());
			if (ImGui::ColoredButton(ICON_REMOVE, ImVec4(0.95f, 0.20f, 0.20f, 0.69f), 3.0f))
			{
				std::scoped_lock lock(m_Mutex);
				std::erase_if(Config().savedDlls, [](auto& dll) {
					return dll.filepath == selectedDLL.filepath;
				});
			}
			ImGui::EndDisabled();

			if (!selectedDLL.name.empty())
				ImGui::ToolTip(std::format("Remove {}", selectedDLL.name).c_str());
			ImGui::PopFont();
			ImGui::EndChild();

			float buttonCenterX = (ImGui::GetContentRegionAvail().x - 150) * 0.5f;
			ImGui::SetCursorPosX(buttonCenterX);
			ImGui::PushFont(Fonts::Title);
			ImGui::BeginDisabled(!selectedProcess.m_Pid || selectedDLL.filepath.empty());
			if (ImGui::Button(ICON_INJECT " Inject", ImVec2(150, 37)))
				ThreadManager::RunDetached([] {
					try
					{
						auto result = PsUtils::Inject(selectedProcess.m_Name, selectedDLL.filepath);
						if (!result.success)
						{
							LOG_ERROR("{}", result.message);
							MsgBox::Error("Error", result.message.c_str());
						}
					}
					catch (const std::exception& e)
					{
						LOG_ERROR("Failed to inject DLL! {}", e.what());
					}
				});
			ImGui::EndDisabled();
			ImGui::PopFont();
		}

	private:
		static inline ProcessList processList;
		static inline std::vector<ProcessEntry> sortedProcessList;
		static inline ProcessEntry selectedProcess;
		static inline DllInfo selectedDLL;
		static inline bool initialized = false;
		static inline char searchBuffer[256];
		static inline std::mutex m_Mutex;
	};
}
