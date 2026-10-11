#include "KeyNames.hpp"
#include <Windows.h>

namespace
{
	struct KeyName
	{
		unsigned int virtualKey;
		const wchar_t* name;
	};

	// Keys that do not produce a character, or whose character would be a confusing name (numpad)
	const KeyName keyNames[] =
	{
		{ VK_LBUTTON, L"Left Mouse" },
		{ VK_RBUTTON, L"Right Mouse" },
		{ VK_MBUTTON, L"Middle Mouse" },
		{ VK_XBUTTON1, L"Mouse X1" },
		{ VK_XBUTTON2, L"Mouse X2" },
		{ VK_BACK, L"Backspace" },
		{ VK_TAB, L"Tab" },
		{ VK_CLEAR, L"Clear" },
		{ VK_RETURN, L"Enter" },
		{ VK_SHIFT, L"Shift" },
		{ VK_CONTROL, L"Ctrl" },
		{ VK_MENU, L"Alt" },
		{ VK_PAUSE, L"Pause" },
		{ VK_CAPITAL, L"Caps Lock" },
		{ VK_ESCAPE, L"Esc" },
		{ VK_SPACE, L"Space" },
		{ VK_PRIOR, L"Page Up" },
		{ VK_NEXT, L"Page Down" },
		{ VK_END, L"End" },
		{ VK_HOME, L"Home" },
		{ VK_LEFT, L"Left arrow" },
		{ VK_UP, L"Up arrow" },
		{ VK_RIGHT, L"Right arrow" },
		{ VK_DOWN, L"Down arrow" },
		{ VK_SELECT, L"Select" },
		{ VK_PRINT, L"Print" },
		{ VK_EXECUTE, L"Execute" },
		{ VK_SNAPSHOT, L"Print Screen" },
		{ VK_INSERT, L"Insert" },
		{ VK_DELETE, L"Delete" },
		{ VK_HELP, L"Help" },
		{ VK_LWIN, L"Left Windows" },
		{ VK_RWIN, L"Right Windows" },
		{ VK_APPS, L"Applications" },
		{ VK_SLEEP, L"Sleep" },
		{ VK_MULTIPLY, L"Numpad *" },
		{ VK_ADD, L"Numpad +" },
		{ VK_SEPARATOR, L"Numpad ," },
		{ VK_SUBTRACT, L"Numpad -" },
		{ VK_DECIMAL, L"Numpad ." },
		{ VK_DIVIDE, L"Numpad /" },
		{ VK_NUMLOCK, L"Num Lock" },
		{ VK_SCROLL, L"Scroll Lock" },
		{ VK_LSHIFT, L"Left Shift" },
		{ VK_RSHIFT, L"Right Shift" },
		{ VK_LCONTROL, L"Left Ctrl" },
		{ VK_RCONTROL, L"Right Ctrl" },
		{ VK_LMENU, L"Left Alt" },
		{ VK_RMENU, L"Right Alt" },
		{ VK_BROWSER_BACK, L"Browser Back" },
		{ VK_BROWSER_FORWARD, L"Browser Forward" },
		{ VK_BROWSER_REFRESH, L"Browser Refresh" },
		{ VK_BROWSER_STOP, L"Browser Stop" },
		{ VK_BROWSER_SEARCH, L"Browser Search" },
		{ VK_BROWSER_FAVORITES, L"Browser Favorites" },
		{ VK_BROWSER_HOME, L"Browser Home" },
		{ VK_VOLUME_MUTE, L"Volume Mute" },
		{ VK_VOLUME_DOWN, L"Volume Down" },
		{ VK_VOLUME_UP, L"Volume Up" },
		{ VK_MEDIA_NEXT_TRACK, L"Next Track" },
		{ VK_MEDIA_PREV_TRACK, L"Previous Track" },
		{ VK_MEDIA_STOP, L"Media Stop" },
		{ VK_MEDIA_PLAY_PAUSE, L"Play/Pause" },
		{ VK_LAUNCH_MAIL, L"Launch Mail" },
		{ VK_LAUNCH_MEDIA_SELECT, L"Launch Media Select" },
		{ VK_LAUNCH_APP1, L"Launch App 1" },
		{ VK_LAUNCH_APP2, L"Launch App 2" },
		{ VK_PROCESSKEY, L"Process" },
		{ VK_ATTN, L"Attn" },
		{ VK_CRSEL, L"CrSel" },
		{ VK_EXSEL, L"ExSel" },
		{ VK_EREOF, L"Erase EOF" },
		{ VK_PLAY, L"Play" },
		{ VK_ZOOM, L"Zoom" },
		{ VK_PA1, L"PA1" },
		{ VK_OEM_CLEAR, L"OEM Clear" },
	};
}

std::wstring GetKeyName(unsigned int virtualKey)
{
	if (virtualKey >= VK_F1 && virtualKey <= VK_F24)
		return L"F" + std::to_wstring(virtualKey - VK_F1 + 1);
	if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
		return L"Numpad " + std::to_wstring(virtualKey - VK_NUMPAD0);

	for (const KeyName& key : keyNames)
	{
		if (key.virtualKey == virtualKey)
			return key.name;
	}

	// Everything else is named after the character it types on the current keyboard layout.
	// The top bit marks dead keys such as ^ on a German keyboard.
	const wchar_t character = static_cast<wchar_t>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_CHAR) & 0x7FFF);
	if (character > L' ')
		return std::wstring(1, character);

	return L"Key " + std::to_wstring(virtualKey);
}
