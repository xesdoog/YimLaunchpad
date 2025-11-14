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


#include "notifier.hpp"


namespace YLP
{
	void Notifier::AddImpl(const std::string& title,
	    const std::string& message,
	    eNotificationLevel level,
	    NotificationCallback callback)
	{
		std::scoped_lock lock(m_Mutex);

		auto now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		std::tm local{};
		localtime_s(&local, &time);
		char buffer[32];
		std::strftime(buffer, sizeof(buffer), "%H:%M", &local);
		std::string header = std::format("{}\t{}", title, buffer);
		std::string ChildID = std::format("##{}{}", static_cast<int>(m_Notifications.size() + 1), title);

		const char* icon{};
		ImVec4 color;
		GetStyle(level, icon, color);

		m_Notifications.emplace_back(Notification{
		    header,
		    message,
		    ChildID,
		    now,
		    level,
		    callback,
		    false,
		    icon,
		    color});

		PlaySoundQueue();
		m_Viewed = false;
	}

	void Notifier::ClearReadImpl()
	{
		std::erase_if(m_Notifications, [](const Notification& n) {
			return n.m_Read;
		});
	}

	void Notifier::PlaySoundQueueImpl()
	{
		auto now = std::chrono::steady_clock::now();
		if (now - m_LastAudioQueueTime < 5s)
			return;

		if (PlaySound(reinterpret_cast<LPCSTR>(notif_audio_data), NULL, SND_MEMORY | SND_ASYNC))
			m_LastAudioQueueTime = now;
	}

	float Notifier::ComputeTotalHeight()
	{
		float total = 60.0f;

		for (auto& n : GetInstance().m_Notifications)
		{
			if (n.m_Read)
				continue;

			total += n.ComputeHeight();
			total += 20.0f;
		}

		return total + 20.f;
	}

	void Notifier::DrawImpl()
	{
		ImVec2 parentWindowSize = ImGui::GetWindowSize();
		ImVec2 parentWindowPos = ImGui::GetWindowPos();
		float maxPopupHeight = parentWindowSize.y * 0.6f;
		ImVec2 popupSize(parentWindowSize.x * 0.6f, 0.0f);
		ImVec2 popupPos(parentWindowPos.x + parentWindowSize.x - popupSize.x - ImGui::GetStyle().WindowPadding.x,
		    ImGui::GetCursorPosY() + 11.0f);
		float contentHeight = ComputeTotalHeight();
		float popupHeight = std::min(contentHeight, maxPopupHeight);

		ImGui::SetNextWindowPos(popupPos);
		ImGui::SetNextWindowBgAlpha(0.f);
		ImGui::SetNextWindowSize(ImVec2(popupSize.x, popupHeight));
		if (!ImGui::BeginPopup("notifierPopup",
		        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground))
		{
			m_IsOpen = false;
			return;
		}

		m_IsOpen = true;
		m_Viewed = true;

		if (m_ShouldClose)
		{
			ImGui::CloseCurrentPopup();
			m_IsOpen = false;
			m_ShouldClose = false;
			ImGui::EndPopup();
			return;
		}

		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		ImVec2 winTL = winPos;
		ImVec2 winBR = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);
		const float popupRounding = 12.0f;
		const ImU32 popupBg = ImGui::GetColorU32(ImVec4(0.09f, 0.10f, 0.11f, 0.96f));
		const ImU32 popupBorder = IM_COL32(255, 255, 255, 14);

		for (int i = 0; i < 4; ++i)
		{
			float pad = 2.0f + i * 2.0f;
			ImU32 col = IM_COL32(0, 0, 0, 20 - i * 4);
			draw->AddRectFilled(ImVec2(winTL.x - pad, winTL.y - pad), ImVec2(winBR.x + pad, winBR.y + pad), col, popupRounding + pad);
		}

		draw->AddRectFilled(winTL, winBR, popupBg, popupRounding);
		draw->AddRect(winTL, winBR, popupBorder, popupRounding, 0, 1.0f);


		if (!m_Notifications.empty())
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
			if (ImGui::Button(ICON_DELETE "Clear All"))
			{
				std::scoped_lock lock(m_Mutex);
				m_Notifications.clear();
			}

			ImGui::Dummy(ImVec2(1, 8));
		}

		{
			std::scoped_lock lock(m_Mutex);

			if (m_Notifications.empty())
			{
				ImGui::TextDisabled("No notifications.");
				ImGui::EndPopup();
				return;
			}

			float yCursor = ImGui::GetCursorScreenPos().y;
			float xLeft = ImGui::GetCursorScreenPos().x;
			float contentW = ImGui::GetContentRegionAvail().x;

			for (auto& n : m_Notifications)
			{
				if (n.m_Read)
					continue;

				n.Draw(xLeft, contentW, draw);
				ImGui::Dummy(ImVec2(1, 12));
			}
			ClearReadImpl();
		}

		ImGui::EndPopup();
	}

	void Notifier::ShowToastImpl()
	{
		// TODO; or not...
	}

	void Notifier::Notification::Dismiss()
	{
		m_Read = true;
		m_Callback = nullptr;
	}

	void Notifier::Notification::Invoke()
	{
		if (m_Callback)
		{
			auto& callback = m_Callback;
			ThreadManager::Run([callback] {
				callback();
			});
		}

		Dismiss();
	}

	float Notifier::Notification::ComputeHeight() const noexcept
	{
		ImGui::PushFont(Fonts::Small);
		float wrapWidth = ImGui::GetContentRegionAvail().x - 20.0f;
		ImVec2 textSize = ImGui::CalcTextSize(m_Message.c_str(), nullptr, false, wrapWidth);
		ImGui::PopFont();

		float panelHeight = std::min(textSize.y + 60.0f, 144.0f);
		return panelHeight;
	}

	void Notifier::Notification::Draw(float xLeft, float contentW, ImDrawList* drawList)
	{
		using namespace std::chrono_literals;

		ImGuiIO& io = ImGui::GetIO();
		auto now_sys = std::chrono::system_clock::now();
		float age_s = std::chrono::duration_cast<std::chrono::duration<float>>(now_sys - m_TimeCreated).count();
		const float animDur = 0.22f;
		float animT = ImSaturate(age_s / animDur);
		float ease = ImSaturate(animT);
		const float cardRounding = 10.0f;
		const float padding = 12.0f;
		const float accentWidth = 8.0f;
		const float titleSpacing = 9.0f;
		const float rightButtonW = 28.0f;
		ImFont* titleFont = Fonts::Bold;
		ImFont* bodyFont = Fonts::Small;
		ImVec2 cursor = ImVec2(xLeft, ImGui::GetCursorScreenPos().y);

		ImGui::PushFont(titleFont);
		ImVec2 titleSize = ImGui::CalcTextSize(m_Title.c_str());
		ImGui::PopFont();

		ImGui::PushFont(bodyFont);
		float wrap = contentW - (padding * 3.0f) - (accentWidth * 2.0f) - rightButtonW;
		ImVec2 bodySize = ImGui::CalcTextSize(m_Message.c_str(), nullptr, false, wrap);
		ImGui::PopFont();

		float cardHeight = std::max(50.0f, titleSize.y + titleSpacing + bodySize.y + (padding * 2));
		float cardWidth = contentW;
		float slideOffset = (1.0f - ease) * 10.0f;
		float alpha = 0.0f + ease;
		ImVec2 cardTL = ImVec2(cursor.x, cursor.y + slideOffset);
		ImVec2 cardBR = ImVec2(cursor.x + cardWidth, cursor.y + slideOffset + cardHeight);

		drawList->AddRectFilled(ImVec2(cardTL.x, cardTL.y + 4.0f), ImVec2(cardBR.x, cardBR.y + 6.5f), IM_COL32(0, 0, 0, (int)(40.0f * alpha)), cardRounding);
		ImU32 cardBg = ImGui::GetColorU32(ImVec4(0.12f, 0.13f, 0.14f, 0.95f * alpha));
		drawList->AddRectFilled(cardTL, cardBR, cardBg, cardRounding);
		drawList->AddRect(cardTL, cardBR, IM_COL32(255, 255, 255, (int)(18.0f * alpha)), cardRounding, 0, 1.0f);

		ImVec2 accentCenter = ImVec2(cardTL.x + padding, cardTL.y + padding);
		ImU32 accentColor = ImGui::GetColorU32(m_Color);
		drawList->AddText(accentCenter, accentColor, m_Icon);

		ImVec2 titlePos = ImVec2(accentCenter.x + accentWidth + 20.0f, cardTL.y + padding);
		ImGui::PushFont(titleFont);
		drawList->AddText(titlePos, IM_COL32(255, 255, 255, (int)(230.0f * alpha)), m_Title.c_str());
		ImGui::PopFont();

		ImVec2 btnPos = ImVec2(cardBR.x - padding - rightButtonW + 6.0f, cardTL.y + padding - 2.0f);
		ImVec2 btnBR = ImVec2(btnPos.x + 20.0f, btnPos.y + 20.0f);
		ImU32 btnBg = IM_COL32(255, 255, 255, (int)(8.0f * alpha));
		drawList->AddRectFilled(btnPos, btnBR, btnBg, 6.0f);
		ImGui::PushFont(Fonts::Small);
		drawList->AddText(ImVec2(btnPos.x + 3.0f, btnPos.y - 1.0f), IM_COL32(200, 60, 60, (int)(230.0f * alpha)), ICON_DELETE);
		ImGui::PopFont();

		ImVec2 bodyPos = ImVec2(accentCenter.x, accentCenter.y + titleSize.y + titleSpacing);
		ImGui::PushFont(bodyFont);
		ImGui::SetCursorScreenPos(ImVec2(bodyPos.x, cardTL.y + padding + titleSize.y + titleSpacing));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, alpha));
		ImGui::TextWrapped("%s", m_Message.c_str());
		ImGui::PopStyleColor();
		ImGui::PopFont();

		ImRect btnRect(btnPos, btnBR);
		if (!m_Read && ImGui::IsMouseHoveringRect(btnRect.Min, btnRect.Max))
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			if (ImGui::IsMouseClicked(0))
				Dismiss();
		}

		ImGui::SetCursorScreenPos(ImVec2(xLeft, cardBR.y + 8.0f));

		if (m_Callback != nullptr)
		{
			ImRect contentRect(ImVec2(cardTL.x, bodyPos.y), cardBR);
			bool hovered = ImGui::IsMouseHoveringRect(contentRect.Min, contentRect.Max);

			if (hovered)
			{
				drawList->AddRectFilled(contentRect.Min, contentRect.Max, IM_COL32(255, 255, 255, (int)(10.0f * alpha)), 0.f);
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				if (ImGui::IsMouseClicked(0))
					Invoke();
			}
		}
	}
}
