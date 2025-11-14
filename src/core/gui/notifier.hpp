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
#pragma comment(lib, "winmm.lib")

#include <playsoundapi.h>
#include <frontend/audio/notif_sfx.hpp>


namespace YLP
{
	using NotificationCallback = std::function<void()>;

	class Notifier : public Singleton<Notifier>
	{
		friend class Singleton<Notifier>;

	public:
		enum eNotificationLevel : uint8_t
		{
			Info,
			Warning,
			Error,
		};

	private:
		void AddImpl(const std::string& title,
		    const std::string& message,
		    eNotificationLevel level,
		    NotificationCallback callback);

		void ShowToastImpl();
		void DrawImpl();
		void ClearReadImpl();
		void PlaySoundQueueImpl();

		struct Notification
		{
			std::string m_Title;
			std::string m_Message;
			std::string m_ChildID;
			std::chrono::system_clock::time_point m_TimeCreated;
			eNotificationLevel m_Level;
			NotificationCallback m_Callback;
			bool m_Read = false;
			const char* m_Icon;
			ImVec4 m_Color;

			void Dismiss();
			void Invoke();
			void Draw(float xLeft, float contentW, ImDrawList* drawList);
			float ComputeHeight() const noexcept;
		};

	public:
		static void Add(const std::string& title,
		    const std::string& message,
		    eNotificationLevel level = Info,
		    NotificationCallback callback = nullptr)
		{
			GetInstance().AddImpl(title, message, level, callback);
		}

		static void Draw()
		{
			GetInstance().DrawImpl();
		}

		static void ShowToast()
		{
			GetInstance().ShowToastImpl();
		}


		static void PlaySoundQueue()
		{
			GetInstance().PlaySoundQueueImpl();
		}

		static void Toggle()
		{
			ImGui::OpenPopup("notifierPopup");
		}

		static bool IsEmpty()
		{
			return GetInstance().m_Notifications.empty();
		}

		static void ClearRead()
		{
			GetInstance().ClearReadImpl();
		}

		static bool IsOpen()
		{
			return GetInstance().m_IsOpen;
		}

		static bool IsViewed()
		{
			return GetInstance().m_Viewed;
		}

		static void GetStyle(eNotificationLevel level, const char*& icon, ImVec4& color)
		{
			switch (level)
			{
			case Info:
				icon = ICON_MESSAGE;
				color = {0.02f, 1.f, 0.027f, 1.f};
				break;
			case Warning:
				icon = ICON_WARNING;
				color = {1.f, 0.76f, 0.027f, 1.f};
				break;
			case Error:
				icon = ICON_ERROR;
				color = {1.f, 0.0068f, 0.0027f, 1.f};
				break;
			default:
				icon = ICON_MESSAGE;
				color = {0.02f, 1.f, 0.027f, 1.f};
				break;
			}
		}

		static void Close()
		{
			GetInstance().m_ShouldClose = true;
		}

		static float ComputeTotalHeight();

	private:
		std::mutex m_Mutex;
		std::vector<Notification> m_Notifications{};
		std::chrono::steady_clock::time_point m_LastAudioQueueTime;
		bool m_IsOpen{false};
		bool m_ShouldClose{false};
		bool m_Viewed{true};
		ImVec2 m_WindowSize{};
		ImVec2 m_WindowPos{};
	};
}
