#pragma once

#include <common.hpp>
#include "renderer.hpp"


using namespace YLP;

namespace ImGuiHelpers
{
    inline void Spinner(const char* label, float radius = 10.0f, float thickness = 2.0f)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext& g = *GImGui;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);

        const int num_segments = 30;
        const float speed = 8.0f;
        float time = static_cast<float>(ImGui::GetTime());
        float start = fmodf(time * speed, IM_PI * 2.0f);
        float a_min = start;
        float a_max = start + IM_PI * 1.5f;

        ImVec4 col1(0.2f, 0.5f, 1.0f, 1.0f);
        ImVec4 col2(0.4f, 0.7f, 1.0f, 1.0f);
        float phase = fmodf(time * 1.5f, 1.0f);
        ImVec4 col = ImLerp(col1, col2, (sinf(phase * IM_PI * 2.0f) * 0.5f) + 0.5f);
        ImU32 color = ImGui::ColorConvertFloat4ToU32(col);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PathClear();

        for (int i = 0; i <= num_segments; i++)
        {
            float a = a_min + (i / (float)num_segments) * (a_max - a_min);
            draw_list->PathLineTo(ImVec2(center.x + cosf(a) * radius,
                center.y + sinf(a) * radius));
        }

        draw_list->PathStroke(color, 0, thickness);
        ImGuiStyle& style = ImGui::GetStyle();

        if (label && label[0] && strncmp(label, "##", 2) != 0)
        {
            ImVec2 text_pos = ImVec2(center.x + (radius * 0.5) + style.ItemSpacing.x, pos.y);
            draw_list->AddText(text_pos, IM_COL32_WHITE, label);
        }
    }

    static void Tooltip(const char* text, ImFont* font=nullptr)
    {
        if (!ImGui::IsItemHovered())
            return;

        if (!font)
            font = Fonts::Regular;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::PushFont(font);
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15);
        ImGui::Text(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        ImGui::PopFont();
        ImGui::PopStyleVar();
    }

    static void HelpMarker(const char* text)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        Tooltip(text);
    }
}
