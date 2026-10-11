#pragma once
#include <Windows.h>
#include "AutoClicker.hpp"
#include "Settings.hpp"

// The application window
class MainDialog
{
public:
	// Shows the window and returns once it was closed
	void Run(HINSTANCE instance);

private:
	static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK AboutProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK KeyButtonProc(HWND button, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK MessageHook(int code, WPARAM wParam, LPARAM lParam);

	INT_PTR HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void OnInitDialog();
	void OnCommand(int id, int notification);
	void OnDestroy();

	void ShowSpeed();
	void ReadEdits();
	void ShowKeyNames();
	void SetRunning(bool running);
	void StartClicking();
	void StopClicking();

	// Choosing a key: the button is clicked and the next key pressed gets bound
	void BeginKeyCapture(int buttonId);
	void EndKeyCapture();
	void OnKeyCaptured(unsigned int virtualKey);

	bool RegisterHotkey();
	void UnregisterHotkey();
	void PollHotkey();
	void OnHotkeyPressed();
	void OnHotkeyReleased();

	HINSTANCE m_instance = nullptr;
	HWND m_dialog = nullptr;
	HICON m_smallIcon = nullptr;
	HHOOK m_messageHook = nullptr;

	Settings m_settings;
	Settings m_savedSettings; // As they are in the .ini file, to only write it when something changed

	AutoClicker m_clicker;
	unsigned int m_runId = 0;
	bool m_running = false; // The window is in its "autoclicker runs" state, with the settings locked

	WNDPROC m_buttonProc = nullptr;    // Window procedure of the key buttons before they were subclassed
	HWND m_capturingButton = nullptr;  // Key button that currently waits for a key press
	unsigned int m_capturedKey = 0;    // Key that was just chosen, until it is released again
	bool m_hotkeyDown = false;         // To tell a press of the hotkey from it being held or auto-repeating
};
