@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "SCRIPT_DIR=%~dp0"

call :main
set "SCRIPT_EXIT_CODE=%ERRORLEVEL%"
if not "%SCRIPT_EXIT_CODE%"=="0" (
    echo.
    echo Build and run failed with exit code %SCRIPT_EXIT_CODE%. Review the errors above.
    pause
)
exit /b %SCRIPT_EXIT_CODE%

:main
cd /d "%SCRIPT_DIR%" || exit /b 1

set "CONFIG=Debug"
set "BUILD_DIR=build"
set "BIN_DIR=%BUILD_DIR%\Bin\%CONFIG%"
set "COMPILER_PACKAGE=Microsoft.VisualStudio.2022.BuildTools"
set "EXE_NAME=ACG_Lab4.exe"

if not exist "Dependencies\Libraries\glfw3.lib" (
    echo Missing bundled GLFW library. Restore the Dependencies folder before continuing.
    exit /b 1
)
if not exist "Dependencies\Binaries\glfw3.dll" (
    echo Missing bundled GLFW DLL. Restore the Dependencies folder before continuing.
    exit /b 1
)

call :find_cmake
if not defined CMAKE_EXE goto install_cmake
rem CMake 3.24+ supports clearing a failed toolchain selection with --fresh.
"%CMAKE_EXE%" --help | findstr /L /C:"--fresh" >nul
if errorlevel 1 goto install_cmake
goto configure

:install_cmake
call :require_winget
if errorlevel 1 exit /b 1
choice /C YN /N /M "Install or update CMake with winget? [Y/N]"
if errorlevel 2 exit /b 1
winget install --id Kitware.CMake -e --source winget --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
    echo CMake installation failed. See the installer output above.
    exit /b 1
)
rem The current terminal may not see the installer's PATH update yet.
if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
) else (
    call :find_cmake
)
if not defined CMAKE_EXE (
    echo Open a new terminal and run this script again so CMake is on PATH.
    exit /b 1
)
"%CMAKE_EXE%" --help | findstr /L /C:"--fresh" >nul
if errorlevel 1 (
    echo CMake 3.24 or newer is required. Open a new terminal after updating CMake.
    exit /b 1
)

:configure
rem Let CMake discover the toolchain; no Visual Studio installation precheck.
call :configure_project
if not errorlevel 1 goto build

rem Only offer compiler setup for toolchain errors, not project configuration errors.
findstr /I /L /C:"could not find any instance of Visual Studio" /C:"CMAKE_CXX_COMPILER" /C:"CMAKE_MAKE_PROGRAM" /C:"does not support platform specification" /C:"is not able to compile a simple test program" /C:"The Windows SDK version" "%BUILD_DIR%\configure.log" >nul
if errorlevel 1 (
    echo CMake configuration failed. See the output above and "%BUILD_DIR%\configure.log".
    exit /b 1
)

call :require_winget
if errorlevel 1 exit /b 1
echo Setup will install the standalone Microsoft C++ Build Tools and Windows SDK.
echo This uses the Visual Studio Installer but does not install the Visual Studio IDE.
choice /C YN /N /M "Install the C++ build tools with winget? [Y/N]"
if errorlevel 2 exit /b 1
winget install --id %COMPILER_PACKAGE% -e --source winget --force --accept-package-agreements --accept-source-agreements --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
if errorlevel 1 (
    echo C++ Build Tools installation did not complete. See the installer output above.
    echo If a restart is required, restart Windows and run this script again.
    exit /b 1
)

rem Rediscover the toolchain instead of reusing a failed compiler or generator cache.
call :configure_project --fresh
if errorlevel 1 (
    echo CMake configuration still failed. See the output above.
    echo If Build Tools is already installed, add its "Desktop development with C++" workload.
    echo Restart Windows if requested by the installer, then run this script again.
    exit /b 1
)

:build
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config "%CONFIG%"
if errorlevel 1 exit /b 1
pushd "%BIN_DIR%" || exit /b 1
start "" "%EXE_NAME%"
if errorlevel 1 (
    popd
    exit /b 1
)
popd
exit /b 0

:find_cmake
set "CMAKE_EXE="
for %%C in (cmake.exe) do if not "%%~$PATH:C"=="" set "CMAKE_EXE=%%~$PATH:C"
if not defined CMAKE_EXE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
exit /b 0

:require_winget
where winget >nul 2>&1
if errorlevel 1 (
    echo winget is unavailable. Install Microsoft's App Installer and run this script again.
    echo Alternatively, install CMake and Microsoft C++ Build Tools manually.
    exit /b 1
)
exit /b 0

:configure_project
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BUILD_DIR%" exit /b 1
"%CMAKE_EXE%" %* -S . -B "%BUILD_DIR%" -A Win32 > "%BUILD_DIR%\configure.log" 2>&1
set "CONFIGURE_RESULT=%ERRORLEVEL%"
type "%BUILD_DIR%\configure.log"
exit /b %CONFIGURE_RESULT%
