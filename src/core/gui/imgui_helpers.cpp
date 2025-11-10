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


#include "imgui_helpers.hpp"


namespace ImGui
{
	void Spinner(const char* label, float radius, float thickness)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;

		ImGuiStyle& style = ImGui::GetStyle();
		ImGuiContext& g = *GImGui;
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 center = ImVec2(pos.x + thickness + radius, pos.y + thickness + radius);

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

		if (label && label[0] && strncmp(label, "##", 2) != 0)
		{
			ImVec2 text_pos = ImVec2(center.x + radius + thickness + style.ItemSpacing.x, pos.y + (radius * 0.5));
			draw_list->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]), label);
		}

		ImGui::Dummy(ImVec2(radius * 2 + style.FramePadding.x + thickness, radius * 2 + style.FramePadding.y + thickness));
	}

	void ToolTip(const char* text, ImFont* font, bool delayed, float textWrapWidth)
	{
		ImGuiHoveredFlags flags = ImGuiHoveredFlags_AllowWhenDisabled;

		if (delayed)
			flags |= ImGuiHoveredFlags_DelayNormal;

		if (!ImGui::IsItemHovered(flags))
			return;

		if (!font)
			font = Fonts::Regular;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
		ImGui::SetNextWindowBgAlpha(0.848f);
		ImGui::PushFont(font);
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(textWrapWidth >= 0.f ? textWrapWidth : ImGui::GetFontSize() * 25);
		ImGui::Text(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
		ImGui::PopFont();
		ImGui::PopStyleVar();
	}

	void HelpMarker(const char* text, ImFont* font)
	{
		ImGui::SameLine();
		ImGui::TextDisabled(ICON_HELP);
		ToolTip(text, font, false);
	}

	void WarningMessage(const char* text)
	{
		ImGui::PushFont(Fonts::Title);
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35);
		ImGui::TextColored(ImVec4(1.0f, 0.7568, 0.027f, 1.0f), ICON_ALERT);
		ImGui::SameLine();
		ImGui::Text("Warning");
		ImGui::PopFont();
		ImGui::PopTextWrapPos();

		ImGui::Dummy(ImVec2(0, 2.f));
		ImGui::TextWrapped(text);
		ImGui::Spacing();
	}

	void TitleText(const char* text, bool separator)
	{
		ImGui::PushFont(Fonts::Title);
		if (separator)
			ImGui::SeparatorText(text);
		else
			ImGui::Text(text);
		ImGui::PopFont();
	}

	void InfoCallout(ImCalloutType type, const std::string& text, float wrapWidth)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (!window)
			return;

		ImVec4 accentColor;
		const char* label{};
		const char* icon{};

		switch (type)
		{
		case ImCalloutType::Note:
			accentColor = ImVec4(0.0f, 0.001f, 0.803f, 1.0f);
			label = "Note";
			icon = ICON_MESSAGE;
			break;
		case ImCalloutType::Warning:
			accentColor = ImVec4(1.0f, 0.7568, 0.027f, 1.0f);
			label = "Warning";
			icon = ICON_ALERT;
			break;
		case ImCalloutType::Important:
			accentColor = ImVec4(0.498f, 0.1f, 1.0f, 1.0f);
			label = "Important";
			icon = ICON_IMPORTANT;
			break;
		}

		if (wrapWidth <= 0.0f)
			wrapWidth = ImGui::GetContentRegionAvail().x - 10.0f;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
		float panelHeight = textSize.y + 30 + ImGui::GetStyle().FramePadding.y * 2;
		float barWidth = 5.0f;

		drawList->AddRectFilled(ImVec2(pos.x, pos.y + 5), ImVec2(pos.x + barWidth, pos.y + panelHeight), ImColor(accentColor));
		ImGui::SetCursorScreenPos(ImVec2(pos.x + barWidth, pos.y));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
		ImGui::BeginChild(("##panel_" + std::to_string((int)type) + "_" + std::to_string(window->GetID(text.c_str()))).c_str(),
		    ImVec2(0, panelHeight),
		    false,
		    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysUseWindowPadding);

		ImGui::PushFont(Fonts::Bold);
		ImGui::TextColored(accentColor, std::format("{} {}", icon, label).c_str());
		ImGui::PopFont();
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
		ImGui::TextWrapped(text.c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
	}

	ImButtonColorScheme MakeButtonColors(ImVec4 baseColor, float hoverFactor, float activeFactor)
	{
		auto Mul = [](ImVec4 c, float f) {
			return ImVec4(c.x * f, c.y * f, c.z * f, c.w);
		};

		ImButtonColorScheme s;
		s.Base = baseColor;
		s.Hover = Mul(baseColor, hoverFactor);
		s.Active = Mul(baseColor, activeFactor);
		return s;
	}

	bool ColoredButton(const char* label, ImVec4 baseColor, float hoverFactor, float activeFactor)
	{
		bool ret = false;
		ImButtonColorScheme scheme = MakeButtonColors(baseColor);

		ImGui::PushStyleColor(ImGuiCol_Button, scheme.Base);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, scheme.Hover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, scheme.Active);
		ret = ImGui::Button(label);
		ImGui::PopStyleColor(3);

		return ret;
	}

	void ImageRounded(
	    ImTextureID texture_id,
	    float diameter,
	    const ImVec2& uv0,
	    const ImVec2& uv1,
	    const ImVec4& tint_col)
	{
		ImVec2 p_min = ImGui::GetCursorScreenPos();
		ImVec2 p_max = ImVec2(p_min.x + diameter, p_min.y + diameter);
		ImGui::GetWindowDrawList()->AddImageRounded(texture_id, p_min, p_max, uv0, uv1, ImGui::GetColorU32(tint_col), diameter * 0.5f);
		ImGui::Dummy(ImVec2(diameter, diameter));
	}

	bool WrappedSelectable(const char* label)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4 hoverColor = style.Colors[ImGuiCol_HeaderHovered];
		ImVec4 activeColor = style.Colors[ImGuiCol_HeaderActive];

		float wrapX = ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x;
		ImGui::PushTextWrapPos(wrapX);

		ImVec2 textPos = ImGui::GetCursorScreenPos();
		ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, false, wrapX - textPos.x);
		labelSize.x += style.FramePadding.x * 2;
		labelSize.y += style.FramePadding.y * 2;
		ImRect rect(textPos, textPos + labelSize);
		bool hovered = ImGui::IsMouseHoveringRect(rect.Min, rect.Max);
		bool clicked = hovered && ImGui::IsMouseClicked(0);
		bool held = hovered && ImGui::IsMouseDown(0);

		if (hovered)
			ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(held ? activeColor : hoverColor), 4.f);

		ImGui::RenderTextWrapped(rect.Min + ImVec2(style.FramePadding.x, style.FramePadding.y), label, nullptr, wrapX - textPos.x);
		ImGui::Dummy(labelSize);
		ImGui::PopTextWrapPos();

		return clicked;
	}
}
