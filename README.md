![](https://img.shields.io/badge/Current%20Release-1.2.0-green) ![](https://img.shields.io/badge/License-MIT-yellow) ![](https://img.shields.io/github/commits-since/Davide244/ClickBlitz/latest)
# ClickBlitz <img src="https://github.com/Davide244/ClickBlitz/raw/d43074632ac54b8b77ad204ad31dfeac34b680fa/ReadmeIcon.png" width="30" height="30">
ClickBlitz is a small, accurate & portable auto-clicker written in C++. This auto-clicker's goal is to reach highly accurate click speeds, while also being easy to use.

It can easily reach the speeds specified by the user, unlike many other autoclickers. It does this by not trusting any system sleep() function to be on time. Every click is due at a fixed point in time. ClickBlitz sleeps on a high-resolution timer until shortly before that point and busy-waits (empty while loop with a clock) through the rest. How early it has to be awake for that is measured all the time while it clicks, because it differs from machine to machine. This gives the accuracy of busy-waiting without keeping a CPU core at 100%.

## Features
- Click speed in clicks per second, or as an interval in milliseconds
- Clicks the left, middle or right mouse button, or presses a keyboard key
- Can stop by itself after a number of clicks
- Hotkey (a key or a side mouse button) that toggles clicking, or clicks for as long as it is held down
- Remembers its settings in a `ClickBlitz.ini` next to the executable (in `%LOCALAPPDATA%\ClickBlitz` when installed from the Microsoft Store)
- Does not react to its own clicks, so it cannot press its own buttons or minimize itself

## Software requirements
- Windows XP *and above*
- A CPU 🐟

## Accuracy
The solution contains a `Benchmark` project that runs the click loop without clicking anything and reports how far off schedule the clicks were. It can also run the pure busy-wait loop of version 1.1.0 for comparison. On an Intel Core i9-13900K with Windows 11:

| Clicks per second | Actual rate, 1.1.0 | Actual rate, now | CPU core used, 1.1.0 | CPU core used, now |
|---|---|---|---|---|
| 10 | 10.000 | 10.000 | 100 % | 1 % |
| 50 | 49.996 | 50.000 | 100 % | 5 % |
| 150 | 166.6 | 150.000 | 100 % | 13 % |
| 500 | 499.7 | 500.000 | 100 % | 50 - 80 % |
| 1000 | 998.8 | 1000.000 | 100 % | 100 % |

At 150 clicks per second both loops trigger 99 % of the clicks within 10 microseconds of their scheduled time. Around 0.2 % of the clicks are more than 0.1 milliseconds late with either loop, the worst ones 1 to 2 milliseconds. Those are moments in which Windows does not let the thread run.

Above roughly 800 clicks per second there is no time left to sleep in, and ClickBlitz busy-waits all the time like it used to.

```
Benchmark.exe --cps 150 --seconds 30
Benchmark.exe --cps 150 --seconds 30 --busywait --priority normal
```

## Building
Open `ClickBlitz.sln` with Visual Studio 2022 or later.

- `Debug` and `Release` are built with the toolset of the Visual Studio you are using. The result runs on the Windows versions that toolset supports (Windows 10 and later for Visual Studio 2026).
- `ReleaseXP` is the build that runs on everything from Windows XP (SP3) to Windows 11. It needs the `v141_xp` toolset. Visual Studio 2026 no longer offers it, but Visual Studio 2022 and the Build Tools for Visual Studio 2022 do, as the components *MSVC v141 - VS 2017 C++ x64/x86 build tools* and *C++ Windows XP Support for VS 2017 (v141) tools*. With those installed, `build-xp.cmd` builds this configuration from the command line.

## Microsoft Store package
`build-msix.cmd` builds `Release` for x64 and packs it into `bin\ClickBlitz_<version>_x64.msix`, the file that is uploaded to the Microsoft Store in [Partner Center](https://partner.microsoft.com/dashboard). Besides Visual Studio it needs the Windows 10 or 11 SDK, for `makepri.exe` and `makeappx.exe`. The package installs on Windows 10 1809 and later; the `ReleaseXP` executable stays the download for everything older.

- `Package\AppxManifest.xml` describes the package. Its `Identity Name`, `Identity Publisher` and `PublisherDisplayName` are the values Partner Center lists for the app under *Product management* > *Product identity*, and the Store only accepts a package that has exactly those.
- The version of the package is `APP_VERSION_NUMBER` from `ClickBlitz.h`. The Store wants its fourth number to be 0, and a higher version with every update.
- The logos in `Package\Assets` were made from `ClickBlitz.ico` by `Package\make-assets.cmd`, which needs [ImageMagick](https://imagemagick.org). It only has to run again after the icon has changed.
- The package is not signed, because the Store signs it. Windows does not install it like that. To try the packaged app first, turn on Developer Mode in the Windows settings and register the folder the package was made from: `Add-AppxPackage -Register obj\Msix\Package\AppxManifest.xml` in PowerShell.

Installed from a package, ClickBlitz keeps its settings in `%LOCALAPPDATA%\ClickBlitz\ClickBlitz.ini`, because the folder it is installed in is read-only.
