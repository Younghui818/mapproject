@echo off
setlocal enabledelayedexpansion

set "OUT=%~1"
set "CACHE=%~2"
set "MODE=%~3"

set "QT_BIN=C:\Qt\6.12.0\msvc2022_64\bin"
set "VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\"

if "%OUT%"==""   set "OUT=C:\Code\mapproject\build\MapStudio\RelWithDebInfo"
if "%CACHE%"=="" set "CACHE=C:\Code\mapproject\build\_qt_runtime_cache"

if /i "%MODE%"=="FULL" goto :FULL

rem ===== 增量同步：只复制新增/变化的文件 =====
if not exist "%CACHE%\Qt6Widgets.dll" (
  echo [deploy] cache missing, skip. Run with FULL first.
  exit /b 0
)
if not exist "%OUT%" mkdir "%OUT%"

robocopy "%CACHE%" "%OUT%" /E /XO /R:1 /W:1 /NJH /NJS /NFL /NDL /NP ^
  /XF MapStudio.exe MapStudio.pdb MapStudio.ilk MapStudio.exp MapStudio.lib vc_redist.x64.exe
set "RC=!errorlevel!"
if !RC! GEQ 8 (
  echo [deploy] robocopy FAILED, code !RC!
  exit /b !RC!
)
echo [deploy] Qt runtime synced to %OUT% (robocopy code !RC!)
exit /b 0

rem ===== 全量部署：只在需要时手动跑一次 =====
:FULL
echo [deploy] full deploy into cache ...
if exist "%CACHE%" rd /s /q "%CACHE%"
mkdir "%CACHE%"
copy /y "%OUT%\MapStudio.exe" "%CACHE%\" >nul

"%QT_BIN%\windeployqt.exe" "%CACHE%\MapStudio.exe" ^
  --dir "%CACHE%" --pdb --force --verbose 1
set "RC=!errorlevel!"
if !RC! NEQ 0 (
  echo [deploy] windeployqt FAILED, code !RC!
  exit /b !RC!
)
type nul > "%CACHE%\.deploy_stamp"
echo [deploy] cache ready: %CACHE%
exit /b 0