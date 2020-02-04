@echo off

if "%__VS_VCVARS64%" == "1" goto :initialized

set __VS_LOCATION=%ProgramFiles(x86)%\Microsoft Visual Studio\2019
set __VS_EDITIONS=Enterprise,Professional,Community
for %%i in (%__VS_EDITIONS%) do (
  if exist "%__VS_LOCATION%\%%i\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%__VS_LOCATION%\%%i\VC\Auxiliary\Build\vcvarsall.bat" x64
    set __VS_VCVARS64=1
    goto :cleanup
  )
)

:cleanup
set __VS_LOCATION=
set __VS_EDITIONS=

:initialized
if %0 == "%~0" (
  goto :open
) else (
  goto :make
)

:make
cmake -E time nmake /nologo system=windows %*
goto :eof

:open
for /f "tokens=*" %%i in ('where code.cmd') do (
  call :code "%%i\.."
  goto :eof
)

:code
set VSCODE_DEV=
set ELECTRON_RUN_AS_NODE=1
start "" /b "%~dp1Code.exe" "%~dp1resources\app\out\cli.js" --new-window "%~dp0"
