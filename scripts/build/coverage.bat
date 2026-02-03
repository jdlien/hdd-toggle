@echo off
REM Run tests with code coverage using OpenCppCoverage
REM Generates HTML report in coverage/ directory
REM
REM Install OpenCppCoverage:
REM   winget install OpenCppCoverage.OpenCppCoverage
REM   -or-
REM   choco install opencppcoverage
REM   -or-
REM   Download from: https://github.com/OpenCppCoverage/OpenCppCoverage/releases

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."

REM Change to project root for consistent paths
pushd "%PROJECT_ROOT%"

REM Check if OpenCppCoverage is available (check both PATH and default install location)
set "COVERAGE_EXE=OpenCppCoverage.exe"
where OpenCppCoverage >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" (
        set "COVERAGE_EXE=C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
    ) else (
        echo ERROR: OpenCppCoverage not found.
        echo.
        echo Install via winget:
        echo   winget install OpenCppCoverage.OpenCppCoverage
        echo.
        echo Or via Chocolatey:
        echo   choco install opencppcoverage
        echo.
        echo Or download from:
        echo   https://github.com/OpenCppCoverage/OpenCppCoverage/releases
        popd
        exit /b 1
    )
)

REM Build tests first if needed (or rebuild if source changed)
echo Building tests with debug symbols...
call scripts\build\compile-tests.bat
if %errorlevel% neq 0 (
    echo Failed to build tests
    popd
    exit /b 1
)

echo.
echo Running tests with coverage instrumentation...
echo.

REM Create coverage output directory
if exist coverage rmdir /s /q coverage
mkdir coverage

REM Get absolute path for coverage tool
for %%i in ("%PROJECT_ROOT%") do set "ABS_ROOT=%%~fi"

REM Run OpenCppCoverage
REM --sources: Only instrument our source files (not system headers)
REM --modules: Only instrument our test executable
REM --export_type: Generate HTML and Cobertura XML reports
"%COVERAGE_EXE%" ^
    --sources "%ABS_ROOT%\include" ^
    --modules "%ABS_ROOT%\tests\run-tests.exe" ^
    --export_type html:coverage ^
    --export_type cobertura:coverage\coverage.xml ^
    -- "%ABS_ROOT%\tests\run-tests.exe" --reporter compact

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo Coverage report generated: coverage\index.html
    echo Cobertura XML: coverage\coverage.xml
    echo ========================================
    echo.

    REM Open the report in default browser
    if "%1"=="--open" (
        start "" "coverage\index.html"
    ) else (
        echo Run with --open flag to auto-open report in browser
    )
) else (
    echo.
    echo Coverage run failed with error code %errorlevel%
)

popd
