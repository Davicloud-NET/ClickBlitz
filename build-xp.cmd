@echo off
rem Builds the executables that also run on Windows XP (configuration ReleaseXP) into bin\Win32\ReleaseXP
rem and bin\x64\ReleaseXP.
rem
rem They need the v141_xp toolset, which Visual Studio 2026 no longer offers. Visual Studio 2022 or its
rem Build Tools still have it, as the components "MSVC v141 - VS 2017 C++ x64/x86 build tools" and
rem "C++ Windows XP Support for VS 2017 (v141) tools".
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo vswhere.exe was not found. Is Visual Studio installed?
	exit /b 1
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -products * -requires Microsoft.VisualStudio.Component.WinXP -latest -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
	echo No Visual Studio installation with the Windows XP toolset ^(v141_xp^) was found.
	exit /b 1
)

"%MSBUILD%" "%~dp0ClickBlitz.sln" /nologo /m /v:minimal /p:Configuration=ReleaseXP /p:Platform=x86 || exit /b 1
"%MSBUILD%" "%~dp0ClickBlitz.sln" /nologo /m /v:minimal /p:Configuration=ReleaseXP /p:Platform=x64 || exit /b 1
