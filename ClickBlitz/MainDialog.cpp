#include "MainDialog.hpp"
#include "ClickBlitz.h"
#include "KeyNames.hpp"
#include "Resource.h"
#include <CommCtrl.h>
#include <algorithm>
#include <string>

namespace
{
	const UINT WM_CLICKING_FINISHED = WM_APP + 1;
	const int HotkeyId = 1;
	const UINT_PTR HotkeyPollTimer = 1;
	const UINT HotkeyPollIntervalMs = 15;
	const UINT ModNoRepeat = 0x4000; // MOD_NOREPEAT, which the headers only declare when targeting Windows 7 or later

	// Controls that are locked while the autoclicker runs
	const int settingControls[] =
	{
		IDC_SPEED, IDC_SPEED_SPIN, IDC_SPEED_UNIT, IDC_STOP_AFTER,
		IDC_TARGET_LEFT, IDC_TARGET_MIDDLE, IDC_TARGET_RIGHT, IDC_TARGET_KEY, IDC_TARGET_KEY_BIND,
		IDC_HOTKEY_BIND, IDC_HOTKEY_TOGGLE, IDC_HOTKEY_HOLD
	};

	bool IsMouseButton(unsigned int virtualKey)
	{
		return virtualKey == VK_XBUTTON1 || virtualKey == VK_XBUTTON2;
	}

	// Shift, Ctrl, Alt and the Windows keys cannot be bound on their own
	bool IsModifierKey(unsigned int virtualKey)
	{
		switch (virtualKey)
		{
		case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
		case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
		case VK_MENU: case VK_LMENU: case VK_RMENU:
		case VK_LWIN: case VK_RWIN:
			return true;
		}
		return false;
	}

	bool IsKeyDown(unsigned int virtualKey)
	{
		return (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
	}

	// White instead of the grey dialog background
	INT_PTR WindowColorBrush(WPARAM deviceContext)
	{
		SetBkColor(reinterpret_cast<HDC>(deviceContext), GetSysColor(COLOR_WINDOW));
		return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
	}
}

void MainDialog::Run(HINSTANCE instance)
{
	m_instance = instance;
	m_settings.Load();
	m_savedSettings = m_settings;

	DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, DialogProc, reinterpret_cast<LPARAM>(this));
}

INT_PTR CALLBACK MainDialog::DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_INITDIALOG)
	{
		SetWindowLongPtrW(dialog, DWLP_USER, lParam);
		reinterpret_cast<MainDialog*>(lParam)->m_dialog = dialog;
	}

	MainDialog* self = reinterpret_cast<MainDialog*>(GetWindowLongPtrW(dialog, DWLP_USER));
	if (self == nullptr)
		return FALSE;
	return self->HandleMessage(message, wParam, lParam);
}

INT_PTR MainDialog::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (message)
	{
	case WM_INITDIALOG:
		OnInitDialog();
		return TRUE;
	case WM_COMMAND:
		OnCommand(LOWORD(wParam), HIWORD(wParam));
		return TRUE;
	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == IDM_ABOUT)
		{
			DialogBoxParamW(m_instance, MAKEINTRESOURCEW(IDD_ABOUTBOX), m_dialog, AboutProc, 0);
			return TRUE;
		}
		// Removing the minimize button alone is not enough: a press on it that began just before
		// the autoclicker was started would still go through when the mouse button is released
		if ((wParam & 0xFFF0) == SC_MINIMIZE && m_running)
			return TRUE;
		return FALSE;
	case WM_HOTKEY:
		// Without MOD_NOREPEAT (Windows XP and Vista) this message repeats while the key is held
		if (wParam == HotkeyId && !m_hotkeyDown)
		{
			m_hotkeyDown = true;
			SetTimer(m_dialog, HotkeyPollTimer, HotkeyPollIntervalMs, nullptr); // To notice the release
			OnHotkeyPressed();
		}
		return TRUE;
	case WM_TIMER:
		if (wParam == HotkeyPollTimer)
			PollHotkey();
		return TRUE;
	case WM_CLICKING_FINISHED:
		if (wParam == m_runId)
			StopClicking();
		return TRUE;
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
		return WindowColorBrush(wParam);
	case WM_CLOSE:
		// Handled here rather than through IDCANCEL, so that Esc does not close the window
		EndDialog(m_dialog, 0);
		return TRUE;
	case WM_DESTROY:
		OnDestroy();
		return TRUE;
	}
	return FALSE;
}

void MainDialog::OnInitDialog()
{
	SendMessageW(m_dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(m_instance, MAKEINTRESOURCEW(IDI_CLICKBLITZ))));
	m_smallIcon = static_cast<HICON>(LoadImageW(m_instance, MAKEINTRESOURCEW(IDI_SMALL), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
	SendMessageW(m_dialog, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_smallIcon));

	const HMENU systemMenu = GetSystemMenu(m_dialog, FALSE);
	AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(systemMenu, MF_STRING, IDM_ABOUT, L"&About " APP_NAME L"...");

	SendDlgItemMessageW(m_dialog, IDC_SPEED_UNIT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"clicks per second"));
	SendDlgItemMessageW(m_dialog, IDC_SPEED_UNIT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"ms between clicks"));
	SendDlgItemMessageW(m_dialog, IDC_SPEED_UNIT, CB_SETCURSEL, static_cast<WPARAM>(m_settings.speedUnit), 0);
	SendDlgItemMessageW(m_dialog, IDC_SPEED, EM_SETLIMITTEXT, 7, 0);
	SendDlgItemMessageW(m_dialog, IDC_STOP_AFTER, EM_SETLIMITTEXT, 9, 0);
	ShowSpeed();
	SetDlgItemInt(m_dialog, IDC_STOP_AFTER, static_cast<UINT>(m_settings.stopAfter), FALSE);

	CheckRadioButton(m_dialog, IDC_TARGET_LEFT, IDC_TARGET_KEY, IDC_TARGET_LEFT + static_cast<int>(m_settings.target));
	CheckRadioButton(m_dialog, IDC_HOTKEY_TOGGLE, IDC_HOTKEY_HOLD, IDC_HOTKEY_TOGGLE + static_cast<int>(m_settings.hotkeyMode));

	const HWND tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_dialog, nullptr, m_instance, nullptr);
	TOOLINFOW speedTip = {};
	speedTip.cbSize = sizeof(speedTip);
	speedTip.hwnd = m_dialog;
	speedTip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	speedTip.uId = reinterpret_cast<UINT_PTR>(GetDlgItem(m_dialog, IDC_SPEED));
	speedTip.lpszText = const_cast<wchar_t*>(L"Note: Most Computers cannot produce more than 750 - 850 clicks a second!");
	SendMessageW(tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&speedTip));

	m_messageHook = SetWindowsHookExW(WH_GETMESSAGE, MessageHook, nullptr, GetCurrentThreadId());

	// The key buttons need to see every key press while they wait for the key to bind
	m_buttonProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(GetDlgItem(m_dialog, IDC_HOTKEY_BIND), GWLP_WNDPROC,
		reinterpret_cast<LONG_PTR>(KeyButtonProc)));
	SetWindowLongPtrW(GetDlgItem(m_dialog, IDC_TARGET_KEY_BIND), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(KeyButtonProc));

	if (!RegisterHotkey())
	{
		const std::wstring text = GetKeyName(m_settings.hotkey) + L" could not be registered as the hotkey. Another program may already be using it.";
		m_settings.hotkey = 0;
		MessageBoxW(m_dialog, text.c_str(), APP_NAME, MB_ICONWARNING | MB_OK);
	}
	ShowKeyNames();
	SetRunning(false);
}

void MainDialog::OnCommand(int id, int notification)
{
	switch (id)
	{
	case IDC_START:
		StartClicking();
		break;
	case IDC_STOP:
		StopClicking();
		break;
	case IDC_SPEED_UNIT:
		if (notification == CBN_SELCHANGE)
		{
			// Each unit remembers its own value
			ReadEdits();
			m_settings.speedUnit = static_cast<SpeedUnit>(SendDlgItemMessageW(m_dialog, IDC_SPEED_UNIT, CB_GETCURSEL, 0, 0));
			ShowSpeed();
		}
		break;
	case IDC_TARGET_LEFT:
	case IDC_TARGET_MIDDLE:
	case IDC_TARGET_RIGHT:
	case IDC_TARGET_KEY:
		if (notification == BN_CLICKED)
		{
			m_settings.target = static_cast<ClickTarget>(id - IDC_TARGET_LEFT);
			if (m_settings.target == ClickTarget::Key && m_settings.targetKey == 0)
				BeginKeyCapture(IDC_TARGET_KEY_BIND);
		}
		break;
	case IDC_HOTKEY_TOGGLE:
	case IDC_HOTKEY_HOLD:
		if (notification == BN_CLICKED)
			m_settings.hotkeyMode = static_cast<HotkeyMode>(id - IDC_HOTKEY_TOGGLE);
		break;
	case IDC_TARGET_KEY_BIND:
	case IDC_HOTKEY_BIND:
		if (notification == BN_CLICKED)
			BeginKeyCapture(id);
		break;
	}
}

void MainDialog::OnDestroy()
{
	m_clicker.Stop();
	KillTimer(m_dialog, HotkeyPollTimer);
	UnregisterHotkey();

	ReadEdits();
	if (m_settings != m_savedSettings)
		m_settings.Save();

	if (m_messageHook != nullptr)
		UnhookWindowsHookEx(m_messageHook);
	if (m_smallIcon != nullptr)
		DestroyIcon(m_smallIcon);
}

// Sees every message of this thread right after it was taken from the queue, and discards the mouse
// and keyboard input that the autoclicker itself generated. Without this, the autoclicker operates
// ClickBlitz whenever the cursor rests on its window: it presses Stop, changes settings, or hits the
// minimize button in the instant between two runs. Locking the controls while it runs cannot fully
// prevent that, because clicks that are already queued get processed after the controls are unlocked.
LRESULT CALLBACK MainDialog::MessageHook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION && wParam == PM_REMOVE)
	{
		MSG* message = reinterpret_cast<MSG*>(lParam);
		const UINT id = message->message;
		const bool isInput = (id >= WM_MOUSEFIRST && id <= WM_MOUSELAST)
			|| (id >= WM_NCMOUSEMOVE && id <= WM_NCXBUTTONDBLCLK)
			|| (id >= WM_KEYFIRST && id <= WM_KEYLAST);
		if (isInput && GetMessageExtraInfo() == static_cast<LPARAM>(Click::Signature))
			message->message = WM_NULL;
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

// Puts the value of the selected speed unit into the edit control
void MainDialog::ShowSpeed()
{
	const bool perSecond = m_settings.speedUnit == SpeedUnit::ClicksPerSecond;
	SendDlgItemMessageW(m_dialog, IDC_SPEED_SPIN, UDM_SETRANGE32, 1, perSecond ? Settings::MaxClicksPerSecond : Settings::MaxIntervalMs);
	SetDlgItemInt(m_dialog, IDC_SPEED, static_cast<UINT>(perSecond ? m_settings.clicksPerSecond : m_settings.intervalMs), FALSE);
}

// Takes over what was typed into the edit controls, corrected to something usable
void MainDialog::ReadEdits()
{
	const bool perSecond = m_settings.speedUnit == SpeedUnit::ClicksPerSecond;
	const int maximum = perSecond ? Settings::MaxClicksPerSecond : Settings::MaxIntervalMs;
	const UINT typedSpeed = GetDlgItemInt(m_dialog, IDC_SPEED, nullptr, FALSE);
	const int speed = static_cast<int>(std::min<UINT>(std::max<UINT>(typedSpeed, 1), static_cast<UINT>(maximum)));
	(perSecond ? m_settings.clicksPerSecond : m_settings.intervalMs) = speed;
	if (static_cast<UINT>(speed) != typedSpeed)
		SetDlgItemInt(m_dialog, IDC_SPEED, static_cast<UINT>(speed), FALSE);

	m_settings.stopAfter = static_cast<int>(GetDlgItemInt(m_dialog, IDC_STOP_AFTER, nullptr, FALSE));
	SetDlgItemInt(m_dialog, IDC_STOP_AFTER, static_cast<UINT>(m_settings.stopAfter), FALSE);
}

void MainDialog::ShowKeyNames()
{
	SetDlgItemTextW(m_dialog, IDC_HOTKEY_BIND, m_settings.hotkey != 0 ? GetKeyName(m_settings.hotkey).c_str() : L"None");
	SetDlgItemTextW(m_dialog, IDC_TARGET_KEY_BIND, m_settings.targetKey != 0 ? GetKeyName(m_settings.targetKey).c_str() : L"None");
}

void MainDialog::SetRunning(bool running)
{
	for (const int id : settingControls)
		EnableWindow(GetDlgItem(m_dialog, id), !running);
	EnableWindow(GetDlgItem(m_dialog, IDC_START), !running);
	EnableWindow(GetDlgItem(m_dialog, IDC_STOP), running);

	// The control that had the keyboard focus may be one of those that just got disabled
	const HWND focus = GetFocus();
	if (focus == nullptr || !IsWindowEnabled(focus))
		SendMessageW(m_dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(GetDlgItem(m_dialog, running ? IDC_STOP : IDC_START)), TRUE);

	// No minimizing while the autoclicker runs (WM_SYSCOMMAND refuses it as well)
	m_running = running;
	LONG_PTR style = GetWindowLongPtrW(m_dialog, GWL_STYLE);
	style = running ? (style & ~WS_MINIMIZEBOX) : (style | WS_MINIMIZEBOX);
	SetWindowLongPtrW(m_dialog, GWL_STYLE, style);
	SetWindowPos(m_dialog, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void MainDialog::StartClicking()
{
	if (m_clicker.IsRunning() || m_capturingButton != nullptr)
		return;

	ReadEdits();
	if (m_settings.target == ClickTarget::Key && m_settings.targetKey == 0)
	{
		MessageBoxW(m_dialog, L"Choose the key to press first.", APP_NAME, MB_ICONINFORMATION | MB_OK);
		return;
	}

	Click click;
	switch (m_settings.target)
	{
	case ClickTarget::MiddleButton:
		click = Click::OfMouseButton(MouseButton::Middle);
		break;
	case ClickTarget::RightButton:
		click = Click::OfMouseButton(MouseButton::Right);
		break;
	case ClickTarget::Key:
		click = Click::OfKey(static_cast<WORD>(m_settings.targetKey));
		break;
	default:
		click = Click::OfMouseButton(MouseButton::Left);
		break;
	}

	const double period = m_settings.speedUnit == SpeedUnit::ClicksPerSecond
		? 1.0 / m_settings.clicksPerSecond
		: m_settings.intervalMs / 1000.0;

	SetRunning(true);
	m_runId = m_clicker.Start(click, period, static_cast<uint64_t>(m_settings.stopAfter), m_dialog, WM_CLICKING_FINISHED);
}

void MainDialog::StopClicking()
{
	m_clicker.Stop();
	SetRunning(false);
}

void MainDialog::BeginKeyCapture(int buttonId)
{
	if (m_capturingButton != nullptr)
		return;

	// Otherwise the current hotkey would arrive as WM_HOTKEY instead of as a key press and could not be chosen again
	UnregisterHotkey();

	m_capturingButton = GetDlgItem(m_dialog, buttonId);
	SetWindowTextW(m_capturingButton, L"Press a key...");
	SetFocus(m_capturingButton);
	SetCapture(m_capturingButton); // Side mouse buttons count as keys, and a click anywhere cancels
}

void MainDialog::EndKeyCapture()
{
	if (m_capturingButton == nullptr)
		return;

	const HWND button = m_capturingButton;
	m_capturingButton = nullptr;
	if (GetCapture() == button)
		ReleaseCapture();

	if (!RegisterHotkey())
	{
		const std::wstring text = GetKeyName(m_settings.hotkey) + L" could not be registered as the hotkey. Another program may already be using it.";
		m_settings.hotkey = 0;
		MessageBoxW(m_dialog, text.c_str(), APP_NAME, MB_ICONWARNING | MB_OK);
	}
	ShowKeyNames();
}

void MainDialog::OnKeyCaptured(unsigned int virtualKey)
{
	const bool forHotkey = GetDlgCtrlID(m_capturingButton) == IDC_HOTKEY_BIND;
	// Keep waiting for a key that can be used. Only the hotkey may be a mouse button.
	if (IsModifierKey(virtualKey) || (IsMouseButton(virtualKey) && !forHotkey))
		return;

	// The autoclicker pressing its own hotkey would switch itself off again
	if (virtualKey == (forHotkey ? m_settings.targetKey : m_settings.hotkey))
	{
		EndKeyCapture();
		MessageBoxW(m_dialog, L"The hotkey and the key to press have to be different keys.", APP_NAME, MB_ICONINFORMATION | MB_OK);
		return;
	}

	if (!IsMouseButton(virtualKey))
		m_capturedKey = virtualKey;

	if (forHotkey)
		m_settings.hotkey = virtualKey;
	else
	{
		m_settings.targetKey = virtualKey;
		m_settings.target = ClickTarget::Key;
		CheckRadioButton(m_dialog, IDC_TARGET_LEFT, IDC_TARGET_KEY, IDC_TARGET_KEY);
	}
	EndKeyCapture();
}

LRESULT CALLBACK MainDialog::KeyButtonProc(HWND button, UINT message, WPARAM wParam, LPARAM lParam)
{
	MainDialog* self = reinterpret_cast<MainDialog*>(GetWindowLongPtrW(GetParent(button), DWLP_USER));

	if (self->m_capturedKey != 0 && !IsKeyDown(self->m_capturedKey))
		self->m_capturedKey = 0;

	if (self->m_capturingButton == button)
	{
		switch (message)
		{
		case WM_GETDLGCODE:
			// Keeps the dialog from using Tab, Enter, Esc and the arrow keys for itself
			return DLGC_WANTALLKEYS;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			self->OnKeyCaptured(static_cast<unsigned int>(wParam));
			return 0;
		case WM_XBUTTONDOWN:
			self->OnKeyCaptured(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
			return TRUE;
		case WM_KEYUP:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
			return 0;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			self->EndKeyCapture();
			return 0;
		case WM_KILLFOCUS:
		case WM_CAPTURECHANGED:
			self->EndKeyCapture();
			break;
		}
	}
	else if (self->m_capturedKey != 0)
	{
		// The key that was just chosen is still held down. If it is Space or Enter, the button
		// must not take its repeats as being pressed again.
		switch (message)
		{
		case WM_GETDLGCODE:
			return DLGC_WANTALLKEYS;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
			return 0;
		}
	}
	return CallWindowProcW(self->m_buttonProc, button, message, wParam, lParam);
}

// Returns false if the hotkey could not be registered
bool MainDialog::RegisterHotkey()
{
	KillTimer(m_dialog, HotkeyPollTimer);
	m_hotkeyDown = false;
	if (m_settings.hotkey == 0)
		return true;

	// RegisterHotKey does not do mouse buttons, those are polled instead. Windows XP and Vista
	// do not know MOD_NOREPEAT and reject it.
	if (!IsMouseButton(m_settings.hotkey)
		&& !RegisterHotKey(m_dialog, HotkeyId, ModNoRepeat, m_settings.hotkey)
		&& !RegisterHotKey(m_dialog, HotkeyId, 0, m_settings.hotkey))
		return false;

	// Right after it was chosen the key is still held down, which must not count as a press
	m_hotkeyDown = IsKeyDown(m_settings.hotkey);
	if (m_hotkeyDown || IsMouseButton(m_settings.hotkey))
		SetTimer(m_dialog, HotkeyPollTimer, HotkeyPollIntervalMs, nullptr);
	return true;
}

void MainDialog::UnregisterHotkey()
{
	KillTimer(m_dialog, HotkeyPollTimer);
	UnregisterHotKey(m_dialog, HotkeyId);
}

// Runs permanently for a mouse button hotkey, and for a keyboard hotkey from its press until its release
void MainDialog::PollHotkey()
{
	const bool down = IsKeyDown(m_settings.hotkey);
	if (down && !m_hotkeyDown)
	{
		m_hotkeyDown = true;
		OnHotkeyPressed();
	}
	else if (!down && m_hotkeyDown)
	{
		m_hotkeyDown = false;
		if (!IsMouseButton(m_settings.hotkey))
			KillTimer(m_dialog, HotkeyPollTimer);
		OnHotkeyReleased();
	}
}

void MainDialog::OnHotkeyPressed()
{
	if (m_capturingButton != nullptr)
		return;

	if (m_settings.hotkeyMode == HotkeyMode::Toggle && m_clicker.IsRunning())
		StopClicking();
	else
		StartClicking();
}

void MainDialog::OnHotkeyReleased()
{
	if (m_settings.hotkeyMode == HotkeyMode::Hold && m_clicker.IsRunning())
		StopClicking();
}

INT_PTR CALLBACK MainDialog::AboutProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemTextW(dialog, IDC_ABOUT_VERSION, APP_NAME L" " APP_VERSION);
		SetDlgItemTextW(dialog, IDC_ABOUT_COPYRIGHT, APP_COPYRIGHT);
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(dialog, LOWORD(wParam));
			return TRUE;
		}
		break;
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
		return WindowColorBrush(wParam);
	}
	return FALSE;
}
