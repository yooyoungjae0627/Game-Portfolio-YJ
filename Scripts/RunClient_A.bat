@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem  RunClientA_Editor.bat (UE Editor -game로 클라 A 실행)
rem  - 서버(127.0.0.1:7777)로 자동 접속 시도
rem  - 창 위치/유저dir 분리
rem  - 로그는 C:\Temp\UE5_Multi_Shooter_Logs 로 강제 저장
rem ============================================================

set "UPROJECT=C:\Game-Portfolio-YJ\UE5_Multi_Shooter\UE5_Multi_Shooter.uproject"
set "UE_EDITOR=C:\UE5_SourceCode\Engine\Binaries\Win64\UnrealEditor.exe"

rem 서버 주소
set "ADDR=127.0.0.1:7777"

rem 로그
set "LOGDIR=C:\Temp\UE5_Multi_Shooter_Logs"
set "LOGFILE=%LOGDIR%\ClientA.log"

rem 창 설정 (A)
set "RESX=800"
set "RESY=600"
set "WINX=50"
set "WINY=50"

rem 저장/설정 충돌 방지용 (A/B 분리)
set "USERDIR=ClientA"

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
echo [RunClientA] Starting Client A (Editor -game)
echo   ADDR    = %ADDR%
echo   LOGFILE = "%LOGFILE%"
echo ============================================================

rem "%ADDR%"를 "첫 맵/URL" 자리로 넣으면 실행과 동시에 open 시도
start "UE5_Multi_Shooter ClientA" "%UE_EDITOR%" "%UPROJECT%" "%ADDR%" ^
  -game -log -windowed -ResX=%RESX% -ResY=%RESY% -WinX=%WINX% -WinY=%WINY% ^
  -UserDir=%USERDIR% ^
  -AbsLog="%LOGFILE%"

endlocal
