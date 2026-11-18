@echo off
rem Opens the OpenCMR2 CIA Builder window. Needs Python 3 from python.org
rem (its installer adds the "py" launcher and the window toolkit).
setlocal
cd /d "%~dp0"

rem pyw/pythonw: the window alone, without this console.
where pyw >/dev/null 2>/dev/null && (start "" pyw -3 cia_builder_gui.py & exit /b 0)
where pythonw >/dev/null 2>/dev/null && (start "" pythonw cia_builder_gui.py & exit /b 0)
where py >/dev/null 2>/dev/null && (py -3 cia_builder_gui.py & exit /b)

echo Python 3 was not found.
echo Install it from https://www.python.org/downloads/ (keep "py launcher" ticked),
echo then double-click this file again.
pause
exit /b 1
