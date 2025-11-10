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

#include <commctrl.h>
#include <playsoundapi.h>

#pragma comment(lib, "winmm.lib") // not fsl lol


namespace YLP
{
	class MsgBox
	{
	private:
		enum class Icon
		{
			Info,
			Warning,
			Error,
			Question,
			None
		};

		enum class Buttons
		{
			OK,
			OKCancel,
			YesNo,
			YesNoCancel
		};

	public:
		static void Info(const std::wstring& title, const std::wstring& message)
		{
			Show(title, message, Buttons::OK, Icon::Info);
		}

		static void Warn(const std::wstring& title, const std::wstring& message)
		{
			Show(title, message, Buttons::OK, Icon::Warning);
		}

		static void Error(const std::wstring& title, const std::wstring& message)
		{
			Show(title, message, Buttons::OK, Icon::Error);
		}

		static bool Confirm(const std::wstring& title, const std::wstring& message)
		{
			int result = Show(title, message, Buttons::YesNo, Icon::Question);
			return result == IDYES;
		}

		static void Info(const std::string& title, const std::string& message)
		{
			Info(Utils::UTF8ToWide(title), Utils::UTF8ToWide(message));
		}

		static void Warn(const std::string& title, const std::string& message)
		{
			Warn(Utils::UTF8ToWide(title), Utils::UTF8ToWide(message));
		}

		static void Error(const std::string& title, const std::string& message)
		{
			Error(Utils::UTF8ToWide(title), Utils::UTF8ToWide(message));
		}

		static bool Confirm(const std::string& title, const std::string& message)
		{
			return Confirm(Utils::UTF8ToWide(title), Utils::UTF8ToWide(message));
		}

	private:
		static int Show(const std::wstring& title,
		    const std::wstring& message,
		    Buttons buttons = Buttons::OK,
		    Icon icon = Icon::Info);

		static PCWSTR GetIcon(Icon icon);
		static UINT GetButtons(Buttons buttons);
		static UINT GetMsgBoxFlags(Buttons buttons);
		static UINT GetMsgBoxIcon(Icon icon);
		static PCWSTR GetSoundAlias(Icon icon);
	};
}
