@echo off
rem Builds the package for the Microsoft Store, bin\ClickBlitz_<version>_x64.msix, out of the x64 Release
rem build, Package\AppxManifest.xml and the logos in Package\Assets.
rem
rem The package is not signed, because the Store signs what is uploaded to it. Windows does not install
rem an unsigned package, so this one is good for uploading only (see README.md).
rem
rem Needs Visual Studio 2022 or later with the C++ tools, and the Windows 10 or 11 SDK, which
rem makepri.exe and makeappx.exe belong to.
setlocal

set "ROOT=%~dp0"
set "WORK=%ROOT%obj\Msix"
set "LAYOUT=%WORK%\Package"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo vswhere.exe was not found. Is Visual Studio installed?
	exit /b 1
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
	echo No Visual Studio installation with the C++ tools was found.
	exit /b 1
)

rem The SDK keeps its tools in one folder per SDK version. The newest one that has both is taken.
set "KITS="
for /f "tokens=1,2,*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 /reg:32 2^>nul') do if "%%a"=="KitsRoot10" set "KITS=%%c"
set "SDKBIN="
if defined KITS for /f "delims=" %%d in ('dir /b /ad /on "%KITS%bin\10.*" 2^>nul') do if exist "%KITS%bin\%%d\x64\makepri.exe" if exist "%KITS%bin\%%d\x64\makeappx.exe" set "SDKBIN=%KITS%bin\%%d\x64"
if not defined SDKBIN (
	echo makepri.exe and makeappx.exe were not found. Is the Windows 10 or 11 SDK installed?
	exit /b 1
)

rem The package gets the version number of the executable: 1,1,0,0 in ClickBlitz.h becomes 1.1.0.0
set "VERSION="
for /f "tokens=2,*" %%a in ('findstr /b /c:"#define APP_VERSION_NUMBER" "%ROOT%ClickBlitz\ClickBlitz.h"') do set "VERSION=%%b"
if not defined VERSION (
	echo APP_VERSION_NUMBER was not found in ClickBlitz\ClickBlitz.h
	exit /b 1
)
set "VERSION=%VERSION: =%"
set "VERSION=%VERSION:,=.%"

rem The Store keeps the fourth number for itself and only takes packages that leave it at 0
set "REVISION="
for /f "tokens=1-5 delims=." %%a in ("%VERSION%") do if "%%e"=="" set "REVISION=%%d"
if not "%REVISION%"=="0" (
	echo APP_VERSION_NUMBER in ClickBlitz\ClickBlitz.h is %VERSION%. The Store wants four numbers, the last of them 0.
	exit /b 1
)

"%MSBUILD%" "%ROOT%ClickBlitz.sln" /nologo /m /v:minimal /t:ClickBlitz /p:Configuration=Release /p:Platform=x64 || exit /b 1

rem Everything that goes into the package is collected in obj\Msix\Package first
if exist "%WORK%" rmdir /s /q "%WORK%"
mkdir "%LAYOUT%\Assets" || exit /b 1
copy /y "%ROOT%bin\x64\Release\ClickBlitz.exe" "%LAYOUT%" >nul || exit /b 1
copy /y "%ROOT%Package\Assets\*.png" "%LAYOUT%\Assets" >nul || exit /b 1

rem PowerShell reads the paths from the environment, which spares quoting them a second time
set "MANIFEST=%ROOT%Package\AppxManifest.xml"
set "PACKED_MANIFEST=%LAYOUT%\AppxManifest.xml"
powershell -NoProfile -Command "$ErrorActionPreference = 'Stop'; $manifest = New-Object Xml.XmlDocument; $manifest.PreserveWhitespace = $true; $manifest.Load($env:MANIFEST); $manifest.Package.Identity.Version = $env:VERSION; $manifest.Save($env:PACKED_MANIFEST)" || exit /b 1

rem resources.pri is how Windows gets from "Assets\Square44x44Logo.png" in the manifest to the file for
rem the scale factor, size and theme it needs. It bears the name of the package, hence /mn.
"%SDKBIN%\makepri.exe" new /pr "%LAYOUT%" /cf "%ROOT%Package\priconfig.xml" /mn "%PACKED_MANIFEST%" /of "%LAYOUT%\resources.pri" /o || exit /b 1

set "PACKAGE=%ROOT%bin\ClickBlitz_%VERSION%_x64.msix"
rem makeappx.exe lists every file it packs, so what it says is only shown if it fails
"%SDKBIN%\makeappx.exe" pack /d "%LAYOUT%" /p "%PACKAGE%" /o >"%WORK%\makeappx.log" 2>&1
if errorlevel 1 (
	type "%WORK%\makeappx.log"
	exit /b 1
)

echo.
echo Package: %PACKAGE%

powershell -NoProfile -Command "$manifest = [xml](Get-Content $env:MANIFEST); if (($manifest.Package.Identity.Name + $manifest.Package.Identity.Publisher + $manifest.Package.Properties.PublisherDisplayName) -like '*PLACEHOLDER*') { exit 1 }"
if errorlevel 1 (
	echo.
	echo Package\AppxManifest.xml still has placeholders where the names of the package and its publisher go.
	echo The Store only accepts the package once they are replaced by the values from Partner Center.
)
exit /b 0
