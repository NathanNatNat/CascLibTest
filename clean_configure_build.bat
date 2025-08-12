@echo off
echo Starting batch script execution...

:: Run clean.bat
echo Running clean.bat...
call clean.bat
if %errorlevel% neq 0 (
    echo clean.bat failed with error code %errorlevel%.
    exit /b %errorlevel%
)

:: Run configure.bat
echo Running configure.bat...
call configure.bat
if %errorlevel% neq 0 (
    echo configure.bat failed with error code %errorlevel%.
    exit /b %errorlevel%
)

:: Run build.bat
echo Running build.bat...
call build.bat
if %errorlevel% neq 0 (
    echo build.bat failed with error code %errorlevel%.
    exit /b %errorlevel%
)

echo All scripts executed successfully.