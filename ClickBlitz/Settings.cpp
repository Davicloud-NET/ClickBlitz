#include "Settings.hpp"
#include "ClickBlitz.h"
#include <Windows.h>
#include <algorithm>
#include <climits>
#include <string>

namespace
{
	const wchar_t Section[] = L"Settings";

	// The Microsoft Store version runs from an MSIX package, which gives its process a package identity.
	// GetCurrentPackageFullName is Windows 8+. The function is looked up at runtime so that the
	// executable still loads on Windows XP.
	bool HasPackageIdentity()
	{
		typedef LONG(WINAPI* GetCurrentPackageFullNameFn)(UINT32*, PWSTR);

		const FARPROC function = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentPackageFullName");
		if (function == nullptr)
			return false;

		// Asked for the length of the name only. A process without a package gets APPMODEL_ERROR_NO_PACKAGE instead.
		UINT32 length = 0;
		return reinterpret_cast<GetCurrentPackageFullNameFn>(function)(&length, nullptr) == ERROR_INSUFFICIENT_BUFFER;
	}

	// The executable's path with the extension replaced by .ini
	std::wstring GetPortableIniPath()
	{
		wchar_t path[MAX_PATH];
		const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
		if (length == 0 || length == MAX_PATH)
			return std::wstring();

		std::wstring iniPath(path, length);
		const size_t dot = iniPath.find_last_of(L".\\/");
		if (dot != std::wstring::npos && iniPath[dot] == L'.')
			iniPath.erase(dot);
		return iniPath + L".ini";
	}

	// %LOCALAPPDATA%\ClickBlitz\ClickBlitz.ini. Windows redirects what a packaged app writes below AppData
	// into storage it keeps for that app alone, and removes it when the app is uninstalled.
	std::wstring GetPackagedIniPath()
	{
		wchar_t localAppData[MAX_PATH];
		const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
			return std::wstring();

		const std::wstring folder = std::wstring(localAppData, length) + L"\\" APP_NAME;
		CreateDirectoryW(folder.c_str(), nullptr); // Fails if the folder is there already, which is fine
		return folder + L"\\" APP_NAME L".ini";
	}

	// A package is installed into a read-only folder, so there the settings cannot stay next to the executable
	std::wstring GetIniPath()
	{
		return HasPackageIdentity() ? GetPackagedIniPath() : GetPortableIniPath();
	}

	int ReadInt(const std::wstring& iniPath, const wchar_t* key, int defaultValue, int minimum, int maximum)
	{
		const int value = static_cast<int>(GetPrivateProfileIntW(Section, key, defaultValue, iniPath.c_str()));
		return std::min(std::max(value, minimum), maximum);
	}

	void WriteInt(const std::wstring& iniPath, const wchar_t* key, int value)
	{
		WritePrivateProfileStringW(Section, key, std::to_wstring(value).c_str(), iniPath.c_str());
	}
}

void Settings::Load()
{
	const std::wstring iniPath = GetIniPath();
	if (iniPath.empty())
		return;

	const Settings defaults;
	speedUnit = static_cast<SpeedUnit>(ReadInt(iniPath, L"SpeedUnit", static_cast<int>(defaults.speedUnit), 0, 1));
	clicksPerSecond = ReadInt(iniPath, L"ClicksPerSecond", defaults.clicksPerSecond, 1, MaxClicksPerSecond);
	intervalMs = ReadInt(iniPath, L"IntervalMs", defaults.intervalMs, 1, MaxIntervalMs);
	stopAfter = ReadInt(iniPath, L"StopAfter", defaults.stopAfter, 0, INT_MAX);
	target = static_cast<ClickTarget>(ReadInt(iniPath, L"Target", static_cast<int>(defaults.target), 0, 3));
	targetKey = static_cast<unsigned int>(ReadInt(iniPath, L"TargetKey", 0, 0, 254));
	hotkey = static_cast<unsigned int>(ReadInt(iniPath, L"Hotkey", 0, 0, 254));
	hotkeyMode = static_cast<HotkeyMode>(ReadInt(iniPath, L"HotkeyMode", static_cast<int>(defaults.hotkeyMode), 0, 1));
}

// Fails silently if the folder is not writable; the settings then simply are not remembered
void Settings::Save() const
{
	const std::wstring iniPath = GetIniPath();
	if (iniPath.empty())
		return;

	WriteInt(iniPath, L"SpeedUnit", static_cast<int>(speedUnit));
	WriteInt(iniPath, L"ClicksPerSecond", clicksPerSecond);
	WriteInt(iniPath, L"IntervalMs", intervalMs);
	WriteInt(iniPath, L"StopAfter", stopAfter);
	WriteInt(iniPath, L"Target", static_cast<int>(target));
	WriteInt(iniPath, L"TargetKey", static_cast<int>(targetKey));
	WriteInt(iniPath, L"Hotkey", static_cast<int>(hotkey));
	WriteInt(iniPath, L"HotkeyMode", static_cast<int>(hotkeyMode));
}

bool Settings::operator==(const Settings& other) const
{
	return speedUnit == other.speedUnit
		&& clicksPerSecond == other.clicksPerSecond
		&& intervalMs == other.intervalMs
		&& stopAfter == other.stopAfter
		&& target == other.target
		&& targetKey == other.targetKey
		&& hotkey == other.hotkey
		&& hotkeyMode == other.hotkeyMode;
}
