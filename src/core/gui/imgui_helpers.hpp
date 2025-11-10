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


namespace ImGui
{
	struct ImButtonColorScheme
	{
		ImVec4 Base;
		ImVec4 Hover;
		ImVec4 Active;
	};

	enum class ImCalloutType
	{
		Note,
		Warning,
		Important
	};

	ImButtonColorScheme MakeButtonColors(
	    ImVec4 baseColor,
	    float hoverFactor = 1.15f,
	    float activeFactor = 0.9f);

	bool ColoredButton(
	    const char* label,
	    ImVec4 baseColor,
	    float hoverFactor = 1.15f,
	    float activeFactor = 0.9f);

	void ImageRounded(
	    ImTextureID texture_id,
	    float diameter,
	    const ImVec2& uv0 = ImVec2(0, 0),
	    const ImVec2& uv1 = ImVec2(1, 1),
	    const ImVec4& tint_col = ImVec4(1, 1, 1, 1));

	void ToolTip(const char* text,
	    ImFont* font = nullptr,
	    bool delayed = true,
	    float textWrapWidth = -1.0f);

	void HelpMarker(const char* text, ImFont* font = nullptr);
	void Spinner(const char* label, float radius = 10.0f, float thickness = 2.0f);
	void WarningMessage(const char* text);
	void TitleText(const char* text, bool separator = false);
	void InfoCallout(ImCalloutType type, const std::string& text, float wrapWidth = 0.0f);
	bool WrappedSelectable(const char* label);
}
