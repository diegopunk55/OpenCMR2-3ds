@echo off
rem Opens the OpenCMR2 CIA Builder window. Needs Python 3.8 or later
rem (python.org: its installer adds the "py" launcher and the window toolkit).
rem   "CIA Builder (debug).bat"      the same, in a console that stays open
rem   "CIA Builder.bat" which        only shows which Python it found
rem   "CIA Builder.bat" check        also checks that the tool loads, then stops
setlocal EnableExtensions
cd /d "%~dp0"

rem Python is searched for in order; the first that runs is used.
set "PYEXE="
set "PYARG="
rem 1. The py launcher: on the PATH, or where python.org installs it.
for /f "delims=" %%P in ('where py.exe 2^>nul') do if not defined PYEXE call :try "%%P" -3
if not defined PYEXE if exist "%SystemRoot%\py.exe" call :try "%SystemRoot%\py.exe" -3
if not defined PYEXE if exist "%LOCALAPPDATA%\Programs\Python\Launcher\py.exe" call :try "%LOCALAPPDATA%\Programs\Python\Launcher\py.exe" -3
rem 2. python.exe on the PATH (the Microsoft Store placeholder fails the test).
if not defined PYEXE for /f "delims=" %%P in ('where python.exe 2^>nul') do if not defined PYEXE call :try "%%P"
rem 3. The registry, where python.org and Store installs are recorded.
for %%K in ("HKCU\Software\Python\PythonCore" "HKLM\Software\Python\PythonCore" "HKLM\Software\WOW6432Node\Python\PythonCore") do (
    if not defined PYEXE for /f "tokens=2,*" %%A in ('reg query %%K /s /v ExecutablePath 2^>nul ^| findstr /i /c:"ExecutablePath"') do if not defined PYEXE call :try "%%B"
)
rem 4. The usual install folders.
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*" "%ProgramFiles%\Python3*" "%SystemDrive%\Python3*") do if not defined PYEXE if exist "%%D\python.exe" call :try "%%D\python.exe"

if not defined PYEXE goto association
if /i "%~1"=="which" (echo Python: "%PYEXE%" %PYARG% & exit /b 0)

rem Checked here first, so that a problem stays on screen.
"%PYEXE%" %PYARG% -c "import tkinter, cia_builder, cia_builder_gui"
if errorlevel 1 goto failed
if /i "%~1"=="check" (echo Python: "%PYEXE%" %PYARG% & echo The CIA Builder loads. & exit /b 0)
if /i "%~1"=="debug" goto debug

rem The window alone, without this console: pythonw.exe or pyw.exe next to it.
set "PYWEXE="
for %%F in ("%PYEXE%") do if exist "%%~dpF%%~nFw%%~xF" set "PYWEXE=%%~dpF%%~nFw%%~xF"
if defined PYWEXE (start "" "%PYWEXE%" %PYARG% cia_builder_gui.py) else (start "" "%PYEXE%" %PYARG% cia_builder_gui.py)
exit /b 0

:debug
echo Python: "%PYEXE%" %PYARG%
echo Running the CIA Builder; its messages appear here.
"%PYEXE%" %PYARG% cia_builder_gui.py
echo.
echo The CIA Builder ended (exit code %errorlevel%).
pause
exit /b

:association
if /i "%~1"=="which" (echo Python: not found, only the .py file association & exit /b 1)
if /i "%~1"=="check" (echo Python: not found & exit /b 1)
rem Not found by name: open it the way a double-click on the .py does.
echo Python was not found by name; opening cia_builder_gui.py with Windows' .py association.
echo If nothing opens, install Python 3 from https://www.python.org/downloads/
start "" "%~dp0cia_builder_gui.py"
if /i "%~1"=="debug" pause
exit /b 0

:failed
echo.
echo The CIA Builder cannot start with "%PYEXE%" %PYARG%: see the error above.
echo "No module named tkinter": reinstall Python from python.org with "tcl/tk" ticked.
pause
exit /b 1

:try
rem A Python that runs and is 3.8 or later; sets PYEXE and PYARG.
"%~1" %~2 -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>nul
if errorlevel 1 exit /b 0
set "PYEXE=%~1"
set "PYARG=%~2"
exit /b 0
