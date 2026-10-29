@echo off
setlocal
cd /d "%~dp0"
set "UW_ROOT=%~dp0"
set "UW_OUT=build\release"
set "UW_CHECK=0"
:args
if "%~1"=="" goto prepare
if /I "%~1"=="--check" (
  set "UW_CHECK=1"
  shift
  goto args
)
if /I "%~1"=="--output" (
  if "%~2"=="" goto usage
  set "UW_OUT=%~2"
  shift
  shift
  goto args
)
if /I "%~1"=="--help" goto usage
echo Unknown argument: %~1
exit /b 2
:usage
echo Usage: build.cmd [--check] [--output build\release]
echo The completed DLL is copied to the project root as UNJIWARE.dll.
exit /b 0
:prepare
set "UW_ZIG=%UW_ROOT%.tools\zig-x86_64-windows-0.15.2\zig.exe"
if not exist "%UW_ZIG%" (
  echo Missing Zig. Run:
  echo   node tools\bootstrap.mjs
  echo   tar -xf .tools\zig.zip -C .tools
  exit /b 2
)
for %%I in ("%UW_OUT%") do set "UW_OUT=%%~fI"
if not exist "%UW_OUT%" mkdir "%UW_OUT%"
set "ZIG_GLOBAL_CACHE_DIR=%UW_ROOT%.tools\zig-cache"
set "ZIG_LOCAL_CACHE_DIR=%UW_OUT%\zig-cache"
set "UW_COMMON=-target x86-windows-gnu -std=c++20 -fms-extensions -O2 -g -Isrc -I.deps/imgui -I.deps/minhook/include"
set "UW_UI=src/state.cpp src/ui.cpp .deps/imgui/imgui.cpp .deps/imgui/imgui_draw.cpp .deps/imgui/imgui_tables.cpp .deps/imgui/imgui_widgets.cpp .deps/imgui/backends/imgui_impl_win32.cpp .deps/imgui/backends/imgui_impl_dx9.cpp"
set "UW_LIBS=-ld3d9 -luser32 -lgdi32 -limm32 -ldwmapi -lbcrypt"
for %%S in (buffer hook trampoline) do (
  "%UW_ZIG%" cc -target x86-windows-gnu -O2 -c ".deps/minhook/src/%%S.c" -o "%UW_OUT%\%%S.obj"
  if errorlevel 1 goto failed
)
"%UW_ZIG%" cc -target x86-windows-gnu -O2 -c .deps/minhook/src/hde/hde32.c -o "%UW_OUT%\hde-hde32.obj"
if errorlevel 1 goto failed
set UW_OBJECTS="%UW_OUT%\buffer.obj" "%UW_OUT%\hook.obj" "%UW_OUT%\trampoline.obj" "%UW_OUT%\hde-hde32.obj"
echo Building x86 preview...
"%UW_ZIG%" c++ %UW_COMMON% %UW_UI% src/preview.cpp %UW_LIBS% -Wl,--subsystem,windows -o "%UW_OUT%\UNJIWARE-preview.exe"
if errorlevel 1 goto failed
echo Building UNJIWARE.dll...
"%UW_ZIG%" c++ %UW_COMMON% %UW_UI% src/platform.cpp src/game.cpp src/renderer.cpp src/dllmain.cpp %UW_OBJECTS% %UW_LIBS% -shared -o "%UW_OUT%\UNJIWARE.dll"
if errorlevel 1 goto failed
echo Building loader and diagnostics...
"%UW_ZIG%" c++ %UW_COMMON% src/loader.cpp -luser32 -o "%UW_OUT%\UNJIWARE-loader.exe"
if errorlevel 1 goto failed
"%UW_ZIG%" c++ %UW_COMMON% tools/abi_probe.cpp src/platform.cpp src/game.cpp src/state.cpp %UW_OBJECTS% -lbcrypt -luser32 -o "%UW_OUT%\UNJIWARE-abi-probe.exe"
if errorlevel 1 goto failed
"%UW_ZIG%" c++ %UW_COMMON% tools/game_probe.cpp src/platform.cpp src/state.cpp %UW_OBJECTS% -lbcrypt -luser32 -o "%UW_OUT%\UNJIWARE-game-probe.exe"
if errorlevel 1 goto failed
"%UW_ZIG%" c++ %UW_COMMON% tools/ui_probe.cpp %UW_UI% %UW_LIBS% -o "%UW_OUT%\UNJIWARE-ui-probe.exe"
if errorlevel 1 goto failed
if not exist "%UW_OUT%\licenses" mkdir "%UW_OUT%\licenses"
copy /y ".deps\imgui\LICENSE.txt" "%UW_OUT%\licenses\Dear-ImGui.txt" >nul
copy /y ".deps\minhook\LICENSE.txt" "%UW_OUT%\licenses\MinHook.txt" >nul
if "%UW_CHECK%"=="0" goto publish
call :probe UNJIWARE-abi-probe.exe abi-probe.txt
if errorlevel 1 goto failed
call :probe UNJIWARE-abi-probe.exe abi-probe-legacy.txt --legacy
if errorlevel 1 goto failed
call :probe UNJIWARE-game-probe.exe game-probe.txt
if errorlevel 1 goto failed
call :probe UNJIWARE-ui-probe.exe ui-probe.txt
if errorlevel 1 goto failed
:publish
copy /y "%UW_OUT%\UNJIWARE.dll" "%UW_ROOT%UNJIWARE.dll" >nul
if errorlevel 1 (
  echo Could not copy root DLL. Close the game if it loaded that file, then rebuild.
  exit /b 1
)
echo Build complete: %UW_OUT%
echo Root DLL: %UW_ROOT%UNJIWARE.dll
exit /b 0
:probe
pushd "%UW_OUT%"
"%UW_OUT%\%~1" %~3 > "%UW_OUT%\%~2" 2>&1
set "UW_PROBE_RESULT=%ERRORLEVEL%"
type "%UW_OUT%\%~2"
popd
exit /b %UW_PROBE_RESULT%
:failed
echo Build or verification failed. The root DLL was not replaced.
exit /b 1
