@echo off
chcp 65001 >nul
cd /d "%~dp0"
tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo Unreal ist noch offen. Bitte zuerst schliessen, dann nochmal starten.
    pause
    exit /b 1
)
echo Baue den Spiel-Code ...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Parry_Parcour_GameEditor Win64 Development "-Project=%~dp0Parry_Parcour_Game.uproject" -WaitMutex
if errorlevel 1 (
    echo.
    echo FEHLER beim Bauen - siehe oben. Unreal wird nicht gestartet.
    pause
    exit /b 1
)
echo Fertig. Starte Unreal ...
start "" "%~dp0Parry_Parcour_Game.uproject"
