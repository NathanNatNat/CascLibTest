@echo off
rem Clean the build directory
if exist build (
    rmdir /s /q build
    if %errorlevel% neq 0 (
        echo Failed to clean the build directory.
        exit /b %errorlevel%
    )
    echo Build directory cleaned successfully.
) else (
    echo Build directory does not exist.
)