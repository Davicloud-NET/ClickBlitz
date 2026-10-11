#pragma once
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <thread>

enum class MouseButton
{
	Left,
	Middle,
	Right
};

// The pair of input events that makes up one click
struct Click
{
	// Travels with every generated event as its "extra info" (see GetMessageExtraInfo). ClickBlitz uses
	// it to keep its own window from reacting to the autoclicker.
	static const ULONG_PTR Signature = 0x434C4B42; // "CLKB"

	INPUT press;
	INPUT release;

	static Click OfMouseButton(MouseButton button);
	static Click OfKey(WORD virtualKey);
};

class AutoClicker
{
public:
	AutoClicker();
	~AutoClicker();
	AutoClicker(const AutoClicker&) = delete;
	AutoClicker& operator=(const AutoClicker&) = delete;

	// Clicks every periodSeconds on a thread of its own, until Stop() is called or clickLimit clicks were made
	// (0 = no limit). In the second case finishedMessage is posted to notifyWindow, with the returned number
	// as wParam so that it can be told apart from the message of an earlier run.
	unsigned int Start(const Click& click, double periodSeconds, uint64_t clickLimit, HWND notifyWindow, UINT finishedMessage);
	void Stop();
	bool IsRunning() const { return m_running.load(); }

private:
	void Run(Click click, double periodSeconds, uint64_t clickLimit, HWND notifyWindow, UINT finishedMessage, unsigned int runId);

	std::thread m_thread;
	HANDLE m_stopEvent; // Signalled together with m_stopRequested, to wake the thread up if it sleeps
	std::atomic<bool> m_stopRequested{ false };
	std::atomic<bool> m_running{ false };
	unsigned int m_runId = 0;
};
