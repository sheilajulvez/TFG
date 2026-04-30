# Descripción de los módulos de `src`

Este documento resume qué hace cada archivo importante dentro de `src`.

> Nota: en esta carpeta hay mezcla de **clases C++**, **estructuras C**, **módulos de OBS** y algunos **prototipos/comentarios**. Por eso, aquí se explica cada archivo por su función real dentro del proyecto.

## ¿Por qué hay archivos `.c`, `.cpp`, `.h` y `.hpp`?

En este proyecto se mezclan varios lenguajes y estilos de implementación porque cada parte encaja mejor con una tecnología distinta:

- `*.c`: se usan para módulos escritos en C puro, que encajan bien con la API de OBS y con código más cercano al sistema.
- `*.cpp`: se usan cuando hace falta C++, por ejemplo para aprovechar clases, `std::vector`, OpenCV, Qt u otras librerías orientadas a objetos.
- `*.h`: suelen ser cabeceras compartidas entre C y C++, con declaraciones de funciones, `struct` y `typedef`.
- `*.hpp`: normalmente indican cabeceras pensadas sobre todo para C++, especialmente cuando declaran clases.

La mezcla no es un error: responde a la evolución del proyecto. Algunas partes empezaron en C por ser más directas para OBS, y otras pasaron a C++ cuando fue útil trabajar con OpenCV, Qt o una estructura más modular.

FUTURO: deberríamos unificarlo??

## Núcleo del plugin


`Cubo-plugin.c`: Es el módulo principal del plugin. Registra el filtro `cube_filter` en OBS y concentra gran parte de la lógica: carga de modelos 3D, detección ArUco, sincronización web, contador regresivo, scoreboard, texturas y renderizado del overlay. 
`Cubo-plugincambios.c` Variante experimental/simplificada del filtro de cubo. Contiene pruebas de geometría básica, creación de texturas y buffers de vértices/caras. FUTURO JOSE ELIMINAR
`plugin-main.c`  Archivo de entrada y registro del plugin en OBS. En este proyecto también contiene prototipos y FUTURO pruebas antiguas, además de la carga del filtro `pixel_art_plugin`. FUTURO
 `plugin-support.h` Cabecera de soporte compartido para el plugin: nombre, versión y funciones auxiliares de logging. 

## Detección ArUco

`aruco_detector.h` Declara la API pública del detector ArUco: creación, destrucción, detección por frame, configuración de diccionario, tamaño, ID y calibración. También define `ArucoResult`, que guarda el resultado de la detección.

`aruco_detector.cpp` Implementa el detector ArUco con OpenCV. Convierte frames de OBS a BGRA, aplica calibración de cámara, detecta marcadores y calcula posición, rotación y orientación. Incluye una variante para filtrar por lista de IDs permitidos. 

## Temporizador de cuenta atrás


`countdown_clock.h` Declara la interfaz del temporizador: estado, duración, arranque, pausa, reinicio, sincronización y cálculo de ángulos de manecillas. 
`countdown_clock.c` Implementa el reloj de cuenta atrás. Mantiene el estado interno (`stopped`, `running`, `paused`, `finished`), calcula el tiempo restante y genera los ángulos para un reloj analógico o una manecilla única. 

## Modelo 3D y geometría

`SJ_3DModel.h` Define la estructura `Mesh` y declara funciones para cargar, renderizar y limpiar mallas 3D. También incluye funciones para cambiar texturas y renderizar en modo reloj.
`SJ_3DModel.c` Implementa la carga de modelos 3D con Assimp, crea buffers de vértices/índices, calcula propiedades de la malla y gestiona la limpieza y sustitución de texturas. 

`Model3DSource.hpp` Clase C++ basada en Qt/OpenGL para cargar y renderizar un modelo 3D con `QOpenGLFunctions_4_5_Core`, VBO, VAO y shader program. 

`Model3D_OBS.c`  Archivo experimental de OBS. Contiene el filtro `pixel_art_plugin` y código de pruebas relacionado con renderizado y pixel art; también conserva prototipos comentados de una fuente 3D.  FUTURO

`Pixel_art_effect.hpp` Declara la clase `PluginEffect`, un filtro de OBS pensado para aplicar un efecto visual con shader y opciones configurables. 
`Pixel_art_effect.cpp` Implementa el filtro `simple_color_overlay` de OBS. Su `filter_video` modifica el frame en RGBA y registra la fuente dentro del módulo.

 `shader.cpp` Prototipo comentado de una clase `Shader` para compilar y enlazar shaders OpenGL desde archivos. No parece formar parte activa del build actual. FUTURO
 `shaderEffect.cpp` Prototipo comentado de una clase `ShaderEffect` que aplicaría un shader a una fuente de OBS. También parece código experimental. FUTURO

## Sincronización web y datos JSON


 `web_sync.h`  Declara la API del sincronizador web con DOMjudge: creación, destrucción, configuración de concurso, autenticación, polling y acceso a equipos del scoreboard. 
`web_sync.c` Implementa la sincronización con la API web en segundo plano, el parseo de resultados y la cache de equipos. 

 `json_utils.h`  Declara utilidades para parsear JSON y fechas (`ISO 8601`, cabecera `Date` HTTP). 
 `json_utils.c`  Implementa las funciones auxiliares de parseo de enteros, cadenas, fechas y búsqueda de objetos JSON. 

## Conversión de formatos de vídeo
FUTURO
 `yuv2bgra.h`  Declara funciones para convertir distintos formatos YUV a BGRA, junto con el tipo de función auxiliar `get_uv_func`. 
 `yuv2bgra.cpp`  Implementa las conversiones de `I420`, `NV12`, `I422`, `YUY2` y otros formatos hacia BGRA para poder procesarlos después con OBS/OpenCV. 

## Prototipos de interfaz Qt
FUTURO
 `test.hpp`  Declara `TestWidget`, un `QDockWidget` de prueba con botón y lógica de interacción básica. 
 `test2.hpp`  Declara `WhiteSquareWidget`, un `QWidget` simple que pinta un cuadrado blanco en `paintEvent`. 

## Prototipos y archivos auxiliares

FUTURO
 `extraer.py`  Script auxiliar en Python. Probablemente se usa para extraer, transformar o preparar datos del proyecto. 


## Resumen rápido de qué se usa de verdad

Los módulos más importantes para el funcionamiento real del plugin son:

- `Cubo-plugin.c`
- `aruco_detector.cpp` / `aruco_detector.h`
- `countdown_clock.c` / `countdown_clock.h`
- `web_sync.c` / `web_sync.h`
- `SJ_3DModel.c` / `SJ_3DModel.h`
- `json_utils.c` / `json_utils.h`
- `yuv2bgra.cpp` / `yuv2bgra.h`

