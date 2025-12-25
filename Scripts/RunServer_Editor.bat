@echo off
setlocal enabledelayedexpansion

rem =======================
rem  USER SETTINGS (너 경로)
rem =======================
set "UPROJECT=C:\Game-Portfolio-YJ\UE5_Multi_Shooter\UE5_Multi_Shooter.uproject"
set "UE_EDITOR=C:\UE5_SourceCode\Engine\Binaries\Win64\UnrealEditor.exe"

rem 실제 파일: C:\Game-Portfolio-YJ\UE5_Multi_Shooter\Content\Map\L_Lobby.umap
set "UMAP_FILE=C:\Game-Portfolio-YJ\UE5_Multi_Shooter\Content\Map\L_Lobby.umap"
set "MAP=/Game/Map/L_Lobby"

set "PORT=7777"
set "LOGDIR=C:\Temp\UE5_Multi_Shooter_Logs"
set "ABSLOG=%LOGDIR%\Server.log"
set "STDOUT=%LOGDIR%\Server_stdout.txt"
set "STDERR=%LOGDIR%\Server_stderr.txt"

rem =======================
rem  DO NOT TOUCH BELOW
rem =======================

echo ============================================================
echo [RunServer] START
echo UPROJECT = "%UPROJECT%"
echo UE_EDITOR= "%UE_EDITOR%"
echo UMAP     = "%UMAP_FILE%"
echo MAP ARG  = "%MAP%?listen"
echo PORT     = %PORT%
echo LOGDIR   = "%LOGDIR%"
echo ============================================================

rem 0) 무조건 멈춤(창 바로 닫히는지 확인용)
echo [RunServer] (Checkpoint) If you see this, the bat is running.
rem pause

rem 1) 경로 검증
if not exist "%UPROJECT%" (
  echo [ERROR] UPROJECT not found: "%UPROJECT%"
  pause
  exit /b 1
)

if not exist "%UE_EDITOR%" (
  echo [ERROR] UnrealEditor.exe not found: "%UE_EDITOR%"
  pause
  exit /b 1
)

if not exist "%UMAP_FILE%" (
  echo [ERROR] Map file not found: "%UMAP_FILE%"
  echo        Your MAP argument may be wrong. Fix folder/name then retry.
  pause
  exit /b 1
)

rem 2) 로그 폴더 자동 생성
if not exist "%LOGDIR%" mkdir "%LOGDIR%"

rem 3) 이전 stdout/stderr 지우고 시작(선택)
del /q "%STDOUT%" "%STDERR%" 1>nul 2>nul

echo [RunServer] Launching UnrealEditor... (this should NOT instantly close)
echo [RunServer] Writing:
echo   AbsLog : "%ABSLOG%"
echo   StdOut : "%STDOUT%"
echo   StdErr : "%STDERR%"

rem 4) 핵심 실행
rem  -server : 서버
rem  -log / -stdout / -FullStdOutLogOutput : 콘솔/표준출력으로도 로그
rem  -forcelogflush : 로그 즉시 플러시(안 찍히는 상황 줄임)
rem  -NullRHI : 서버에서 렌더링 완전 차단(그래픽/드라이버로 튕김 방지)
rem  -unattended -NoSound : 서버 자동화 옵션
"%UE_EDITOR%" "%UPROJECT%" "%MAP%?listen" ^
  -server -log -stdout -FullStdOutLogOutput -forcelogflush ^
  -port=%PORT% -unattended -NoSound -NullRHI ^
  -AbsLog="%ABSLOG%" ^
  1>>"%STDOUT%" 2>>"%STDERR%"

set "EC=%errorlevel%"
echo ============================================================
echo [RunServer] UnrealEditor exited. ERRORLEVEL=%EC%
echo [RunServer] Check these files NOW:
echo   "%ABSLOG%"
echo   "%STDOUT%"
echo   "%STDERR%"
echo ============================================================
pause

endlocal
