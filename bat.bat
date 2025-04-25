cd obs-plugintemplate-master
cmake --preset windows-x64
echo BUILD DEPENDENCIES
cd .deps
cd obs-studio-31.0.0
cmake -B build_x64 -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Debug;Release;RelWithDebInfo"
cmake --build build_x64 --config Debug
cmake --build build_x64 --config Release
cmake --build build_x64 --config RelWithDebInfo
cd ..
cd ..
echo BUILD PLUGIN
cmake --build --preset windows-x64 --config Debug
cmake --build --preset windows-x64 --config Release
cmake --build --preset windows-x64 --config RelWithDebInfo

REM Finalizar
echo Proceso completado.
pause
