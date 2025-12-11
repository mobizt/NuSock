@echo off
:: Batch file to run Python script in this folder with admin rights

:: Check for admin rights
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as Administrator...
) else (
    echo Requesting Administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

:: Change to the folder where the batch file is located
cd /d "%~dp0"

:: Run the Python script (replace script.py with your filename)
python keygen.py

pause
