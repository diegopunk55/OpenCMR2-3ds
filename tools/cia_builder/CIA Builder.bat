@echo off
rem Opens the OpenCMR2 CIA Builder window. Needs Python 3 from python.org
rem (its installer adds the "py" launcher and the window toolkit).
rem "CIA Builder (debug).bat" runs it in a console that stays open.
setlocal
cd /d "%~dp0"

set "PY="
where py >/dev/null 2>/dev/null && set "PY=py -3" && set "PYW=pyw -3"
if not defined PY where python >/dev/null 2>/dev/null && set "PY=python" && set "PYW=pythonw"
if not defined PY goto nopython

rem Checked here first, so that a problem stays on screen.
%PY% -c "import tkinter, cia_builder, cia_builder_gui"
if errorlevel 1 goto failed

if /i "%~1"=="debug" goto debug
rem The window alone, without this console.
start "" %PYW% cia_builder_gui.py
exit /b 0

:debug
echo Running the CIA Builder; its messages appear here.
%PY% cia_builder_gui.py
echo.
echo The CIA Builder ended (exit code %errorlevel%).
pause
exit /b

:failed
echo.
echo The CIA Builder cannot start: see the error above.
echo "No module named tkinter": reinstall Python from python.org with "tcl/tk" ticked.
pause
exit /b 1

:nopython
echo Python 3 was not found.
echo Install it from https://www.python.org/downloads/ (keep "py launcher" ticked),
echo then double-click this file again.
pause
exit /b 1
