#include "traycontextmenu.hpp"

#include "ttberror.hpp"
#include "ttblog.hpp"

long TrayContextMenu::TrayCallback(WPARAM, LPARAM lParam)
{
	if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP)
	{
		for (const auto &refreshFunction : m_RefreshFunctions)
		{
			refreshFunction();
		}

		POINT pt;
		if (!GetCursorPos(&pt))
		{
			LastErrorHandle(Error::Level::Log, L"Failed to get cursor position.");
		}

		if (!SetForegroundWindow(m_Window))
		{
			Log::OutputMessage(L"Failed to set window as foreground window.");
		}

		SetLastError(0);
		unsigned int item = TrackPopupMenu(GetSubMenu(m_Menu, 0), TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, pt.x, pt.y, 0, m_Window, NULL);
		if (!item && GetLastError() != 0)
		{
			LastErrorHandle(Error::Level::Log, L"Failed to open context menu.");
			return 0;
		}

		const auto &callbackVector = m_MenuCallbackMap[item];
		if (callbackVector.size() > 0)
		{
			for (const auto &callbackPair : callbackVector)
			{
				callbackPair.second();
			}
		}
	}
	return 0;
}

TrayContextMenu::TrayContextMenu(MessageWindow &window, wchar_t *iconResource, wchar_t *menuResource, const HINSTANCE &hInstance) :
	TrayIcon(window, iconResource, 0, hInstance)
{
	m_Menu = LoadMenu(hInstance, menuResource);
	if (!m_Menu)
	{
		LastErrorHandle(Error::Level::Fatal, L"Failed to load context menu.");
	}

	m_Cookie = RegisterTrayCallback(std::bind(&TrayContextMenu::TrayCallback, this, std::placeholders::_1, std::placeholders::_2));
}

TrayContextMenu::MENUCALLBACKCOOKIE TrayContextMenu::RegisterContextMenuCallback(unsigned int item, const callback_t &callback)
{
	unsigned short secret = Util::GetRandomNumber<unsigned short>();
	m_MenuCallbackMap[item].push_back(std::make_pair(secret, callback));

	return (static_cast<MENUCALLBACKCOOKIE>(secret) << 32) + item;
}

bool TrayContextMenu::UnregisterContextMenuCallback(MENUCALLBACKCOOKIE cookie)
{
	unsigned int item = cookie & 0xFFFFFFFF;
	unsigned short secret = (cookie >> 32) & 0xFFFF;

	auto &callbackVector = m_MenuCallbackMap[item];
	for (auto &callbackPair : callbackVector)
	{
		if (callbackPair.first == secret)
		{
			std::swap(callbackPair, callbackVector.back());
			callbackVector.pop_back();
			return true;
		}
	}

	return false;
}

void TrayContextMenu::RefreshBool(unsigned int item, HMENU menu, const bool &value, BoolBindingEffect effect)
{
	if (effect == Toggle)
	{
		CheckMenuItem(menu, item, MF_BYCOMMAND | (value ? MF_CHECKED : MF_UNCHECKED));
	}
	else if (effect == ControlsEnabled)
	{
		EnableMenuItem(menu, item, MF_BYCOMMAND | (value ? MF_ENABLED : MF_GRAYED));
	}
}

void TrayContextMenu::RefreshEnum(HMENU menu, unsigned int first, unsigned int last, unsigned int position)
{
	CheckMenuRadioItem(menu, first, last, position, MF_BYCOMMAND);
}

TrayContextMenu::~TrayContextMenu()
{
	m_Window.UnregisterCallback(m_Cookie);
	if (!DestroyMenu(m_Menu))
	{
		LastErrorHandle(Error::Level::Log, L"Failed to destroy menu");
	}
}