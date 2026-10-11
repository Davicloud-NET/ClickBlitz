// ClickBlitz.cpp : Defines the entry point for the application.
//

#include <Windows.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include "ClickBlitz.h"
#include "MainDialog.hpp"

namespace
{
	bool IsWindowsVistaOrLater()
	{
		OSVERSIONINFOEXW version = {};
		version.dwOSVersionInfoSize = sizeof(version);
		version.dwMajorVersion = 6;
		return VerifyVersionInfoW(&version, VER_MAJORVERSION, VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL)) != FALSE;
	}
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// The mutex is never closed, so it exists for as long as this instance runs
	CreateMutexW(nullptr, FALSE, L"ClickBlitz.Instance");
	if (GetLastError() == ERROR_ALREADY_EXISTS
		&& MessageBoxW(nullptr, L"Another instance of ClickBlitz is already running. Do you want to open a new instance anyways?",
			APP_NAME, MB_ICONEXCLAMATION | MB_YESNO | MB_TOPMOST) != IDYES)
		return 0;

	// The themed controls of Windows XP do not render properly on the white window background,
	// so visual styles are switched off there
	if (!IsWindowsVistaOrLater())
		SetThemeAppProperties(0);

	INITCOMMONCONTROLSEX commonControls;
	commonControls.dwSize = sizeof(commonControls);
	commonControls.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS | ICC_BAR_CLASSES;
	InitCommonControlsEx(&commonControls);

	MainDialog dialog;
	dialog.Run(hInstance);
	return 0;
}
