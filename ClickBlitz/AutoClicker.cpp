#include "AutoClicker.hpp"
#include "ClickLoop.hpp"

namespace
{
	// Keys the keyboard reports with the 0xE0 scan code prefix. Without the flag some of them
	// would arrive as their numpad twin (e.g. Insert as Numpad 0).
	bool IsExtendedKey(WORD virtualKey)
	{
		switch (virtualKey)
		{
		case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
		case VK_DIVIDE: case VK_NUMLOCK: case VK_SNAPSHOT:
		case VK_RCONTROL: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_APPS:
			return true;
		}
		return virtualKey >= VK_BROWSER_BACK && virtualKey <= VK_LAUNCH_APP2;
	}
}

Click Click::OfMouseButton(MouseButton button)
{
	Click click = {};
	click.press.type = INPUT_MOUSE;
	click.press.mi.dwExtraInfo = Signature;
	click.release.type = INPUT_MOUSE;
	click.release.mi.dwExtraInfo = Signature;
	switch (button)
	{
	case MouseButton::Left:
		click.press.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		click.release.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		break;
	case MouseButton::Middle:
		click.press.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
		click.release.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
		break;
	case MouseButton::Right:
		click.press.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
		click.release.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
		break;
	}
	return click;
}

Click Click::OfKey(WORD virtualKey)
{
	Click click = {};
	click.press.type = INPUT_KEYBOARD;
	click.press.ki.wVk = virtualKey;
	// Games tend to read scan codes rather than virtual keys
	click.press.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));
	click.press.ki.dwFlags = IsExtendedKey(virtualKey) ? KEYEVENTF_EXTENDEDKEY : 0;
	click.press.ki.dwExtraInfo = Signature;
	click.release = click.press;
	click.release.ki.dwFlags |= KEYEVENTF_KEYUP;
	return click;
}

AutoClicker::AutoClicker()
	: m_stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
{
}

AutoClicker::~AutoClicker()
{
	Stop();
	CloseHandle(m_stopEvent);
}

unsigned int AutoClicker::Start(const Click& click, double periodSeconds, uint64_t clickLimit, HWND notifyWindow, UINT finishedMessage)
{
	Stop(); // Also collects the thread of a run that ended on its own

	m_stopRequested.store(false);
	ResetEvent(m_stopEvent);
	m_running.store(true);
	m_runId++;
	m_thread = std::thread(&AutoClicker::Run, this, click, periodSeconds, clickLimit, notifyWindow, finishedMessage, m_runId);
	return m_runId;
}

void AutoClicker::Stop()
{
	if (!m_thread.joinable())
		return;

	m_stopRequested.store(true);
	SetEvent(m_stopEvent);
	m_thread.join();
}

void AutoClicker::Run(Click click, double periodSeconds, uint64_t clickLimit, HWND notifyWindow, UINT finishedMessage, unsigned int runId)
{
	// Waking up on time matters more than fairness here, and the thread sleeps for most of its life.
	// With only one core, however, a time critical thread that ends up spinning (very short
	// intervals do that) would starve the window, so the priority is left alone there.
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	if (systemInfo.dwNumberOfProcessors > 1)
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	PrecisionWait wait;
	INPUT inputs[2] = { click.press, click.release };
	RunClickLoop(wait, periodSeconds * static_cast<double>(wait.TicksPerSecond()), clickLimit, m_stopEvent, m_stopRequested,
		[&inputs](int64_t)
		{
			SendInput(2, inputs, sizeof(INPUT));
		});

	m_running.store(false);
	if (!m_stopRequested.load())
		PostMessageW(notifyWindow, finishedMessage, runId, 0);
}
