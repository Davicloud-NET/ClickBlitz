@echo off
rem Makes the logos in Package\Assets out of ClickBlitz\ClickBlitz.ico with ImageMagick (magick.exe).
rem
rem Only needed after the icon has changed: the logos are checked in, so that building the package
rem does not need ImageMagick.
setlocal

set "ICON=%~dp0..\ClickBlitz\ClickBlitz.ico"
set "ASSETS=%~dp0Assets"

where magick >nul 2>&1
if errorlevel 1 (
	echo ImageMagick ^(magick.exe^) was not found on the PATH.
	exit /b 1
)

rem All logos are scaled down from the largest picture in the icon
set "FRAME="
set "LARGEST=0"
for /f "tokens=1,2" %%a in ('magick identify -format "%%s %%w\n" "%ICON%"') do call :consider %%a %%b
if not defined FRAME (
	echo Could not read %ICON%
	exit /b 1
)

if not exist "%ASSETS%" mkdir "%ASSETS%"
del /q "%ASSETS%\*.png" 2>nul

rem The icon as the taskbar, the Start menu, Alt+Tab and Explorer show it, in every size they ask for.
rem Without these Windows takes the 44x44 logo and puts a coloured plate behind it. The margin of a
rem sixteenth on each side is the one the small pictures in the icon have.
for %%n in (16 20 24 30 32 36 40 48 60 64 72 80 96 256) do call :icon %%n || exit /b 1

rem The logos Windows does put on a plate (Start menu of Windows 10, tiles, the Store), for every display
rem scale factor. The plate is not part of the picture, so the icon gets room around it: it takes up
rem three quarters of the small logos and a third of the tile.
for %%s in (100 125 150 200 400) do (
	call :logo Square44x44Logo 44 %%s 3 4 || exit /b 1
	call :logo Square150x150Logo 150 %%s 1 3 || exit /b 1
	call :logo StoreLogo 50 %%s 3 4 || exit /b 1
)

echo Logos written to %ASSETS%
exit /b 0

rem %1 = index of a picture in the icon, %2 = its width
:consider
if %2 gtr %LARGEST% (
	set "LARGEST=%2"
	set "FRAME=%1"
)
exit /b 0

rem %1 = size in pixels
:icon
set /a "INNER=%1 - 2 * (%1 / 16)"
call :render Square44x44Logo.targetsize-%1_altform-unplated.png %1 %INNER% || exit /b 1
rem The same picture again for the light theme, which otherwise falls back to the logo on a plate
copy /y "%ASSETS%\Square44x44Logo.targetsize-%1_altform-unplated.png" "%ASSETS%\Square44x44Logo.targetsize-%1_altform-lightunplated.png" >nul
exit /b %errorlevel%

rem %1 = name, %2 = size in pixels at 100 %, %3 = scale factor in percent,
rem %4 / %5 = fraction of the logo that the icon takes up
:logo
set /a "SIZE=(%2 * %3 + 50) / 100"
set /a "INNER=SIZE * %4 / %5"
call :render %1.scale-%3.png %SIZE% %INNER%
exit /b %errorlevel%

rem %1 = file name, %2 = size of the picture, %3 = size of the icon within it. The date is left out
rem of the file, so that making the logos again from the same icon gives the same files.
:render
magick "%ICON%[%FRAME%]" -trim +repage -resize %3x%3 -background none -gravity center -extent %2x%2 -strip -define png:exclude-chunk=date,time "PNG32:%ASSETS%\%1"
exit /b %errorlevel%
