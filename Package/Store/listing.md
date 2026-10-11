# Microsoft Store listing

Texts to paste into Partner Center, and the two screenshots in this folder (`screenshot-1.png`, `screenshot-2.png`, 1920 x 1080).

## Description

```
ClickBlitz is a small auto-clicker that clicks at the speed you actually asked for.

That sounds like the bare minimum, but a lot of auto-clickers get it wrong. They tell Windows to wait a few milliseconds between clicks, and Windows is not very punctual about that. Set them to 150 clicks per second and you get something in the neighbourhood of 150.

ClickBlitz keeps its own schedule. Every click has a moment at which it is due, and ClickBlitz is already awake and waiting when that moment comes. Ask for 150 clicks per second and you get 150. On my PC, 99 out of 100 clicks land within a hundredth of a millisecond of where they belong, and at ordinary speeds that takes only a small slice of one processor core.

What you can set:

- How fast: clicks per second, or the pause between clicks in milliseconds
- What to click: the left, middle or right mouse button, or a key on the keyboard
- A hotkey to start and stop. Press it once to toggle, or switch to hold mode and it only clicks while you keep the key down. The side buttons of your mouse work as hotkeys too.
- A number of clicks after which it stops by itself

Your settings are still there the next time you open it. ClickBlitz also ignores its own clicks, so leaving the cursor over its window won't make it press its own Stop button or minimize itself.

There are no ads, no accounts and no internet connection. It is one small window, and when you close it, it is gone.

Two things worth knowing before you install:

- It cannot click inside programs that run as administrator. Windows does not let ordinary apps do that.
- Plenty of online games don't allow auto-clickers. Check the rules before you use it there.

ClickBlitz is open source under the MIT license. The code, and a version for older Windows all the way back to XP, are on GitHub: https://github.com/Davicloud-NET/ClickBlitz
```

## Short description

```
A small auto-clicker that clicks at exactly the speed you set. Mouse buttons or keyboard keys, a hotkey that toggles or holds, no ads and no internet connection.
```

## Product features

One per line in Partner Center.

```
Click speed in clicks per second, or as a pause in milliseconds
Clicks the left, middle or right mouse button, or presses a keyboard key
Hotkey to start and stop: toggle, or click only while it is held down
Side mouse buttons can be used as the hotkey
Stops by itself after a set number of clicks
Remembers your settings
Ignores its own clicks, so it cannot press its own buttons
No ads, no account, no internet connection
Open source (MIT license)
```

## Search terms

```
auto clicker
autoclicker
mouse clicker
auto key presser
fast clicker
cps
click automation
```

## What's new in this version

```
First version in the Microsoft Store.
```

## Other fields

- Category: Utilities & tools
- Copyright: Copyright (c) 2024 David Jonjic
- Privacy policy URL: https://github.com/Davicloud-NET/ClickBlitz/blob/master/PRIVACY.md
- Website: https://github.com/Davicloud-NET/ClickBlitz

## Submission options (not shown to customers)

Why the package needs the restricted capability `runFullTrust`:

```
ClickBlitz is a classic Win32 desktop application. It sends mouse clicks and key presses to other windows with SendInput and registers a global hotkey, which a sandboxed app cannot do.
```

Notes for certification:

```
ClickBlitz is an auto-clicker. To test it: set a click speed, click the button next to "Start / stop" and press a key (for example F6) to make it the hotkey, then press that key while the mouse is over any window to start clicking, and press it again to stop. The Start and Stop buttons do the same. No account, sign-in or network access is needed.
```
