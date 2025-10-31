#include "gui.hpp"
#include "../../features/test.hpp"


namespace YLP
{
    static float EaseInOutCubic(float t)
    {
        return t < 0.5f
            ? 4.0f * t * t * t
            : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    }

    void GUI::InitImpl()
    {
        Fonts::Load(ImGui::GetIO());
        SetupStyle();

        AddTab(ICON_FA_BUG, [] { DrawTest(); }, "Testing Testing");
        AddTab(ICON_FA_DATABASE, [] { ImGui::Text("Transition Test"); }, "dummy 1");
        AddTab(ICON_FA_DRIVE, [] {}, "dummy 2");
        AddTab(ICON_FA_COG, [] {}, "Settings");

        m_ActiveTab = m_Tabs.front().get();
    }

    bool GUI::AddTabImpl(const char* name, GuiCallBack&& callback, std::optional<const char*> hint)
    {
        for (const auto& tab : m_Tabs)
        {
            if (tab->m_Name == name)
                return false;
        }

        m_Tabs.push_back(std::make_shared<Tab>(Tab{ name, std::move(callback), hint }));
        return true;
    }

    void GUI::SwitchTabImpl(Tab* newtab, int tabindex)
    {
        if (m_ActiveTab && m_ActiveTab != newtab)
        {
            m_Transition.m_OldIndex = m_TabIndex;
            m_Transition.m_NewIndex = tabindex;
            m_Transition.m_Progress = 0.0f;
            m_Transition.m_StartTime = ImGui::GetTime();
            m_Transition.m_IsAnimating = true;
            m_Transition.m_DirectionRight = (m_TabIndex > tabindex);
            m_ActiveTab = newtab;
        }
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
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
        );

        ImGui::PushFont(Fonts::Regular);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(10.f, 10.f));

        DrawTabBar();

        DrawTabCallback();

        DrawDebugConsole();

        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::End();
    }

    void GUI::DrawTabBarImpl()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        int tabCount = static_cast<int>(m_Tabs.size());
        float spacing = style.ItemSpacing.x;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float totalSpacing = (tabCount - 1) * spacing;
        float preferredWidth = 80.0f;
        float totalPreferred = tabCount * preferredWidth + (tabCount - 1) * spacing;
        float buttonWidth;

        if (availWidth >= totalPreferred)
            buttonWidth = preferredWidth;
        else {
            float shrinkWidth = (availWidth - (tabCount - 1) * spacing) / tabCount;
            buttonWidth = ImMax(60.0f, shrinkWidth);
        }

        float totalWidth = tabCount * buttonWidth + (tabCount - 1) * spacing;
        float startX = ImMax(0.0f, (availWidth - totalWidth) * 0.5f);

        ImGui::SetNextWindowBgAlpha(0.7);
        if (ImGui::BeginChild("##navbar", ImVec2(0, 55.0f), ImGuiChildFlags_Border))
        {
            ImGui::PushFont(Fonts::Bold);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);

            for (int i = 0; i < tabCount; i++)
            {
                auto& tab = *m_Tabs[i];
                bool selected = (m_ActiveTab && m_ActiveTab == &tab);

                ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.25f, 0.35f, 0.55f, 1.0f)
                    : ImVec4(0.15f, 0.15f, 0.18f, 1.0f));

                if (ImGui::Button(tab.m_Name, ImVec2(buttonWidth, 35)))
                    SwitchTab(&tab, i);

                if (tab.m_Hint.has_value())
                    ImGuiHelpers::Tooltip(tab.m_Hint.value());
                ImGui::PopStyleColor();

                if (i < tabCount - 1)
                    ImGui::SameLine(0, spacing);
            }

            ImGui::PopStyleVar();
            ImGui::PopFont();
        }
        ImGui::EndChild();
    }

    void GUI::DrawTabCallbackImpl()
    {
        const float lowerChildHeight = std::min(m_WindowSize.y * 0.4f, 280.0f);
        float cbChildWidth;
        float cbChildHeight = m_DebugConsole ? m_WindowSize.y - lowerChildHeight : 0;
        auto& t = m_Transition;
        float regionWidth = ImGui::GetContentRegionAvail().x;
        float elapsed = static_cast<float>(ImGui::GetTime() - t.m_StartTime);
        static float prevWidth = 0.0f;

        if (t.m_IsAnimating)
        {
            t.m_Progress = ImClamp(elapsed / t.m_Duration, 0.0f, 1.0f);
            float eased = EaseInOutCubic(t.m_Progress);
            float phase = (eased < 0.5f)
                ? 1.0f - (eased * 2.0f)
                : (eased - 0.5f) * 2.0f;

            if (t.m_DirectionRight)
            {
                cbChildWidth = 0;
                float dummyChildWidth = -ImLerp(prevWidth, regionWidth * phase, ImGui::GetIO().DeltaTime * 20);
                prevWidth = dummyChildWidth;
                ImGui::SetNextWindowBgAlpha(0.f);
                ImGui::BeginChild("##dummykid", ImVec2(dummyChildWidth * 1.2, cbChildHeight));
                ImGui::EndChild();
                ImGui::SameLine();
            }
            else
            {
                cbChildWidth = ImLerp(prevWidth, regionWidth * phase, ImGui::GetIO().DeltaTime * 20);
                prevWidth = cbChildWidth;
            }

            if (t.m_Progress >= 1.0f)
            {
                t.m_IsAnimating = false;
                m_TabIndex = t.m_NewIndex;
            }
        }
        else
            cbChildWidth = 0;

        ImGui::SetNextWindowBgAlpha(0.f);
        if (ImGui::BeginChild("##main", ImVec2(cbChildWidth, cbChildHeight), ImGuiChildFlags_Border))
        {
            if (m_ActiveTab && m_ActiveTab->m_Callback && !m_Transition.m_IsAnimating)
                m_ActiveTab->m_Callback();
        }
        ImGui::EndChild();
    }

    void GUI::DrawDebugConsoleImpl()
    {
        if (!m_DebugConsole)
            return;

        ImGui::Spacing();
        if (ImGui::BeginChild("##console", ImVec2(0, 0), ImGuiChildFlags_Border))
        {
            ImGui::BulletText("Output");
            ImGui::SameLine(0, 20);
            ImGui::PushFont(Fonts::Small);

            if (ImGui::Button(ICON_FA_COPY))
            {
                std::string text;
                for (const auto& e : Logger::Entries())
                    text += "[" + e.timestamp + "] " + Logger::ToString(e.level) + " " + e.message + "\n";
                ImGui::SetClipboardText(text.c_str());
            }
            ImGuiHelpers::Tooltip("Copy all log entries");

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_CANCEL_CIRCLE))
                Logger::Clear();
            ImGuiHelpers::Tooltip("Clear all log entries");

            if (ImGui::BeginChild("##debug_output", ImVec2(0, 0), true))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                ImGui::PushTextWrapPos(0.0f);

                for (const auto& entry : Logger::Entries()) {
                    ImVec4 color;
                    switch (entry.level) {
                    case Logger::eLogLevel::Info:  color = ImVec4(0.7f, 0.7f, 0.7f, 1.f); break;
                    case Logger::eLogLevel::Warn:  color = ImVec4(1.f, 0.8f, 0.f, 1.f);   break;
                    case Logger::eLogLevel::Error: color = ImVec4(1.f, 0.3f, 0.3f, 1.f);  break;
                    case Logger::eLogLevel::Debug: color = ImVec4(0.5f, 0.8f, 1.f, 1.f);  break;
                    }

                    auto text = std::string("[" + entry.timestamp + "] " + Logger::ToString(entry.level) + " " + entry.message);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::Selectable(text.c_str(), false);
                    ImGui::PopStyleColor();

                    ImGuiHelpers::Tooltip("Left click to copy");
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

    void GUI::SetupStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        ImGui::StyleColorsDark();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.ChildRounding = 5.0f;
        style.TabRounding = 4.0f;
        style.FramePadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.GrabRounding = 3.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.3f;
        style.PopupBorderSize = 1.0f;

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

    }
}
