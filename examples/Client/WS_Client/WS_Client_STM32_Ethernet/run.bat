@echo off

:: Change to the folder where the batch file is located
cd /d "%~dp0"

:: Run the Python script (replace script.py with your filename)
python simple_server.py

pause
