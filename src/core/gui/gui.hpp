#pragma once

#include "renderer.hpp"


using GuiCallBack = std::function<void()>;

namespace YLP
{
	class GUI final
	{
	private:
		GUI() {};
	public:
		GUI(const GUI&) = delete;
		GUI(GUI&&) noexcept = delete;
		GUI& operator=(const GUI&) = delete;
		GUI& operator=(GUI&&) noexcept = delete;

		struct Tab
		{
			const char* m_Name;
			GuiCallBack m_Callback;
			std::optional<const char*> m_Hint;
		};

		static void Init()
		{
			GetInstance().InitImpl();
		}

		static bool IsUsingKeyboard() // what did I write this for???
		{
			return ImGui::GetIO().WantTextInput;
		}

		static bool AddTab(const char* name, GuiCallBack&& callback, std::optional<const char*> hint)
		{
			return GetInstance().AddTabImpl(name, std::move(callback), hint);
		}

		static void SwitchTab(Tab* newtab, int newindex)
		{
			GetInstance().SwitchTabImpl(newtab, newindex);
		}

		static void Draw()
		{
			GetInstance().DrawImpl();
		}

		static void DrawTabBar()
		{
			GetInstance().DrawTabBarImpl();
		}

		static void DrawTabCallback()
		{
			GetInstance().DrawTabCallbackImpl();
		}

		static void DrawDebugConsole()
		{
			GetInstance().DrawDebugConsoleImpl();
		}

		static void SetupStyle();

	private:
		void InitImpl();
		void DrawImpl();
		void DrawTabBarImpl();
		void DrawTabCallbackImpl();
		void DrawDebugConsoleImpl();
		bool AddTabImpl(const char* name, GuiCallBack&& callback, std::optional<const char*> hint);
		void SwitchTabImpl(Tab* newtab, int newindex);

		bool m_DebugConsole = true;
		int m_TabIndex = 0;

		ImVec2 m_WindowSize;
		std::vector<std::shared_ptr<Tab>> m_Tabs;
		Tab* m_ActiveTab = nullptr;

		struct TabTransition
		{
			int m_OldIndex = -1;
			int m_NewIndex = -1;
			float m_Progress = 0.0f;
			float m_Duration = 0.2f;
			double m_StartTime = 0.0;
			bool m_IsAnimating = false;
			bool m_DirectionRight = false;
		};

		TabTransition m_Transition;

		static GUI& GetInstance()
		{
			static GUI i{};
			return i;
		}
	};
}
