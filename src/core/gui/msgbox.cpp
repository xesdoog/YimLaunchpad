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

#include <common.hpp>
#include "msgbox.hpp"


namespace YLP
{
	int MsgBox::Show(const std::wstring& title,
	    const std::wstring& message,
	    Buttons buttons,
	    Icon icon)
	{
		using TaskDialogIndirect_t = HRESULT(WINAPI*)(
		    const TASKDIALOGCONFIG*,
		    int*,
		    int*,
		    BOOL*);

		HMODULE hComCtl = LoadLibraryW(L"comctl32.dll");
		if (hComCtl)
		{
			auto pTaskDialogIndirect = reinterpret_cast<TaskDialogIndirect_t>(GetProcAddress(hComCtl, "TaskDialogIndirect"));

			if (pTaskDialogIndirect)
			{
				TASKDIALOGCONFIG config = {sizeof(TASKDIALOGCONFIG)};
				config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS;
				config.pszWindowTitle = title.c_str();
				config.pszMainInstruction = message.c_str();
				config.pszMainIcon = GetIcon(icon);
				config.dwCommonButtons = GetButtons(buttons);

				int buttonPressed = 0;
				HRESULT hr = pTaskDialogIndirect(&config, &buttonPressed, nullptr, nullptr);
				FreeLibrary(hComCtl);

				if (SUCCEEDED(hr))
				{
					PlaySoundW(GetSoundAlias(icon), nullptr, SND_ALIAS | SND_ASYNC);
					return buttonPressed;
				}
			}
		}

		UINT flags = GetMsgBoxFlags(buttons) | GetMsgBoxIcon(icon);
		PlaySoundW(GetSoundAlias(icon), nullptr, SND_ALIAS | SND_ASYNC);
		return MessageBoxW(g_Hwnd, message.c_str(), title.c_str(), flags);
	}

	PCWSTR MsgBox::GetIcon(Icon icon)
	{
		switch (icon)
		{
		case Icon::Info: return TD_INFORMATION_ICON;
		case Icon::Warning: return TD_WARNING_ICON;
		case Icon::Error: return TD_ERROR_ICON;
		case Icon::Question: return TD_SHIELD_ICON;
		default: return nullptr;
		}
	}

	UINT MsgBox::GetButtons(Buttons buttons)
	{
		switch (buttons)
		{
		case Buttons::OK: return TDCBF_OK_BUTTON;
		case Buttons::OKCancel: return TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
		case Buttons::YesNo: return TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
		case Buttons::YesNoCancel: return TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
		default: return TDCBF_OK_BUTTON;
		}
	}

	UINT MsgBox::GetMsgBoxFlags(Buttons buttons)
	{
		switch (buttons)
		{
		case Buttons::OK: return MB_OK;
		case Buttons::OKCancel: return MB_OKCANCEL;
		case Buttons::YesNo: return MB_YESNO;
		case Buttons::YesNoCancel: return MB_YESNOCANCEL;
		default: return MB_OK;
		}
	}

	UINT MsgBox::GetMsgBoxIcon(Icon icon)
	{
		switch (icon)
		{
		case Icon::Info: return MB_ICONINFORMATION;
		case Icon::Warning: return MB_ICONWARNING;
		case Icon::Error: return MB_ICONERROR;
		case Icon::Question: return MB_ICONQUESTION;
		default: return 0;
		}
	}

	PCWSTR MsgBox::GetSoundAlias(Icon icon)
	{
		switch (icon)
		{
		case Icon::Info: return L"SystemAsterisk";
		case Icon::Warning: return L"SystemExclamation";
		case Icon::Error: return L"SystemHand";
		case Icon::Question: return L"SystemQuestion";
		default: return L"SystemDefault";
		}
	}
}
