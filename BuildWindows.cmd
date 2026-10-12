@echo off
setlocal
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJ=%~dp0VirtualStadium.uproject"
if not exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo ERROR: Unreal Engine not found. Set UE_ROOT to your UE 5.8.3 installation.
    exit /b 1
)
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" VirtualStadiumEditor Win64 Development -Project="%PROJ%" -WaitMutex
if errorlevel 1 exit /b 1
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%~dp0Saved\Packaged" -prereqs -utf8output
if errorlevel 1 exit /b 1
echo Windows package created under "%~dp0Saved\Packaged".
