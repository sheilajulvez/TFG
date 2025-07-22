@echo off
setlocal

REM --------------------------------------------------------
REM 1) Asegurarnos de que tenemos pip actualizado
REM --------------------------------------------------------
echo Actualizando pip...
python -m pip install --upgrade pip
if errorlevel 1 (
  echo Error al actualizar pip.
  exit /b 1
)

REM --------------------------------------------------------
REM 2) Instalar dependencias: OpenCV y NumPy
REM --------------------------------------------------------
echo Instalando dependencias...
pip install opencv-python numpy
if errorlevel 1 (
  echo Error al instalar dependencias.
  exit /b 1
)

echo Dependencias instaladas. Esperando 5 segundos...
timeout /t 5 /nobreak >nul

REM --------------------------------------------------------
REM 3) Comprobar argumentos
REM --------------------------------------------------------
if "%~3"=="" (
  echo Uso: run_calibrate.bat ^<carpeta_imagenes^> ^<tamaño_cuadrado^> ^<salida_yaml^>
  echo Ejemplo: run_calibrate.bat calib_images 0.025 calibration.yml
  pause
  exit /b 1
)

REM --------------------------------------------------------
REM 4) Ejecutar el script de calibración
REM --------------------------------------------------------
echo Ejecutando calibrate.py...
python calibrate.py "%~1" %~2 "%~3"
if errorlevel 1 (
  echo El script devolvió un error.
  pause
  exit /b 1
)

echo Script completado. Esperando 5 segundos antes del cierre...
timeout /t 5 /nobreak >nul

echo Listo.
pause
endlocal
