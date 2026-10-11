#pragma once

enum class SpeedUnit
{
	ClicksPerSecond,
	IntervalMs
};

enum class ClickTarget
{
	LeftButton,
	MiddleButton,
	RightButton,
	Key
};

enum class HotkeyMode
{
	Toggle, // Press to start, press again to stop
	Hold    // Clicks for as long as the hotkey is held down
};

// Everything the user can set up in the window. Kept in ClickBlitz.ini next to the executable.
struct Settings
{
	static constexpr int MaxClicksPerSecond = 10000;
	static constexpr int MaxIntervalMs = 3600000;

	SpeedUnit speedUnit = SpeedUnit::ClicksPerSecond;
	int clicksPerSecond = 150;
	int intervalMs = 1;
	int stopAfter = 0;                  // Number of clicks after which to stop, 0 = never
	ClickTarget target = ClickTarget::LeftButton;
	unsigned int targetKey = 0;         // Virtual key code pressed when target is Key, 0 = none chosen yet
	unsigned int hotkey = 0;            // Virtual key code that starts and stops clicking, 0 = none
	HotkeyMode hotkeyMode = HotkeyMode::Toggle;

	void Load();
	void Save() const;
	bool operator==(const Settings& other) const;
	bool operator!=(const Settings& other) const { return !(*this == other); }
};
