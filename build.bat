@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   MyICC - GPU Color Control Build Script
echo ============================================
echo.

set "SRC_FILES=src\main.cpp src\nvapi_wrapper.cpp src\color_controller.cpp src\gamma.cpp src\color_enhance.cpp"
set "INC_FLAGS=-Ithirdparty\nvapi -Isrc"
set "LIBS=-luser32 -lgdi32 -lcomctl32 -lpsapi -lshell32"
set "OUT_EXE=myicc.exe"

:: ---- Try MinGW-w64 (w64devkit or MSYS2) ----
set MINGW_PATH=
for %%p in (
    "%TEMP%\w64devkit\bin\g++.exe"
    "C:\msys64\mingw64\bin\g++.exe"
    "C:\mingw64\bin\g++.exe"
) do (
    if exist %%p (
        set MINGW_PATH=%%p
        goto :found_mingw
    )
)
where g++.exe >nul 2>&1 && set MINGW_PATH=g++

:found_mingw
if not "%MINGW_PATH%"=="" (
    echo [INFO] Building with MinGW-w64: %MINGW_PATH%
    "%MINGW_PATH%" -std=c++20 -O0 -static -mwindows -municode %INC_FLAGS% %SRC_FILES% %LIBS% -o %OUT_EXE%
    if !ERRORLEVEL! equ 0 (
        echo [OK] Build successful: %OUT_EXE%
        exit /b 0
    )
    echo [WARN] MinGW build failed
)

:: ---- Try Zig ----
set ZIG_PATH=
for %%p in ("%USERPROFILE%\.local\bin\zig.exe" "%TEMP%\zig\zig.exe") do (
    if exist %%p set ZIG_PATH=%%p
)
where zig >nul 2>&1 && set ZIG_PATH=zig

if not "%ZIG_PATH%"=="" (
    echo [INFO] Building with Zig...
    "%ZIG_PATH%" c++ -target x86_64-windows-gnu -O0 -std=c++20 %INC_FLAGS% %SRC_FILES% %LIBS% -o %OUT_EXE%
    if !ERRORLEVEL! equ 0 (
        echo [OK] Build successful: %OUT_EXE%
        exit /b 0
    )
    echo [WARN] Zig build failed
)

:: ---- Try MSVC ----
where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [INFO] Building with MSVC...
    cl /EHsc /O2 /std:c++20 /Fe:%OUT_EXE% /I thirdparty\nvapi /I src %SRC_FILES% /link user32.lib gdi32.lib comctl32.lib psapi.lib shell32.lib
    if !ERRORLEVEL! equ 0 (
        echo [OK] Build successful: %OUT_EXE%
        exit /b 0
    )
    echo [WARN] MSVC build failed
)

echo.
echo [ERROR] No working C++ compiler found.
echo Install one of these lightweight options:
echo   - w64devkit: https://github.com/skeeto/w64devkit/releases (~50MB, portable)
echo   - Zig: https://ziglang.org/download/ (~70MB, portable)
echo   - MSYS2 MinGW: https://www.msys2.org/
echo.
echo After installing, add the bin/ directory to PATH.
pause
exit /b 1
