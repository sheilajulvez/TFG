@echo off
REM Establecer ruta de trabajo
cd obs-plugintemplate-master

REM Paso 1: Ejecutar cmake para configurar el template con el preset windows-x64
echo Configurando CMake para el template...
cmake --preset windows-x64

cmake --build --preset windows-x64 --config Debug
cmake --build --preset windows-x64 --config Release
cmake --build --preset windows-x64 --config RelWithDebInfo

REM Paso 4: Copiar la .dll y .pdb generados a la carpeta de instalación de OBS
echo Copiando archivos .dll y .pdb a la instalación de OBS...
copy /y "C:\Users\Sheila Julvez\Desktop\U\TFG\TFG\obs-plugintemplate-master\build_x64\RelWithDebInfo\SheilaJosePluginTest.dll" "C:\ProgramData\OBS\obs-studio\obs-plugins\64bit\"
copy /y "C:\Users\Sheila Julvez\Desktop\U\TFG\TFG\obs-plugintemplate-master\build_x64\RelWithDebInfo\SheilaJosePluginTest.pdb" "C:\ProgramData\OBS\obs-studio\obs-plugins\64bit"
copy /y "C:\Users\Sheila Julvez\Desktop\U\TFG\TFG\obs-plugintemplate-master\data" "C:\ProgramData\OBS\obs-studio\data\obs-plugins\SheilaJosePluginTest"
REM Finalizar
echo Proceso completado.
pause
