@echo off
setlocal

cd /d "%~dp0obs-domjudge-ar-overlay" || exit /b 1

echo CONFIGURE
cmake --preset windows-x64

echo BUILD DEPENDENCIES
cmake --build --preset windows-x64 --target deps

echo BUILD PLUGIN
cmake --build --preset windows-x64 --config Debug
cmake --build --preset windows-x64 --config Release
cmake --build --preset windows-x64 --config RelWithDebInfo

echo Proceso completado.
pause
