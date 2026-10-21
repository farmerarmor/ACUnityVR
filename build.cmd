@echo off
rem Run from an x64 Native Tools Command Prompt for Visual Studio 2022.
cmake -S "%~dp0." -B "%~dp0build" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo %*
if errorlevel 1 exit /b 1
cmake --build "%~dp0build" --target ACUnityVRNative ACUnityShaderRegistry ACUnityVRHost
exit /b %errorlevel%
