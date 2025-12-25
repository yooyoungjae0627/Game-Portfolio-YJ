@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem  RunClientB_Editor.bat (UE Editor -game로 클라 B 실행)
rem  - A와 동일, 단 창 위치/유저dir만 다름
rem ============================================================

set "UPROJECT=C:\Game-Portfolio-YJ\UE5_Multi_Shooter\UE5_Multi_Shooter.uproject"
set "UE_EDITOR=C:\UE5_SourceCode\Engine\Binaries\Win64\UnrealEditor.exe"

set "ADDR=127.0.0.1:7777"

set "LOGDIR=C:\Temp\UE5_Multi_Shooter_Logs"
set "LOGFILE=%LOGDIR%\ClientB.log"

rem 창 설정 (B)
set "RESX=800"
set "RESY=600"
set "WINX=900"
set "WINY=50"

set "USERDIR=ClientB"

rem ------------------------------------------------------------

if not exist "%UPROJECT%" (
  echo [ERROR] UPROJECT not found:
  echo   "%UPROJECT%"
  pause
  exit /b 1
)

if not exist "%UE_EDITOR%" (
  echo [ERROR] UnrealEditor.exe not found:
  echo   "%UE_EDITOR%"
  pause
  exit /b 1
)

if not exist "%LOGDIR%" (
  mkdir "%LOGDIR%"
)

echo ============================================================
echo [RunClientB] Starting Client B (Editor -game)
echo   ADDR    = %ADDR%
echo   LOGFILE = "%LOGFILE%"
echo ============================================================

start "UE5_Multi_Shooter ClientB" "%UE_EDITOR%" "%UPROJECT%" "%ADDR%" ^
  -game -log -windowed -ResX=%RESX% -ResY=%RESY% -WinX=%WINX% -WinY=%WINY% ^
  -UserDir=%USERDIR% ^
  -AbsLog="%LOGFILE%"

endlocal
