# OBS DOMjudge AR Overlay

## Trabajo de Fin de Grado

Desarrollo de un plugin nativo para **OBS Studio** orientado a la visualizacion de **realidad aumentada**, **modelos 3D** y **datos en tiempo real de concursos de programacion** integrados sobre la senal de video.

Este repositorio recoge tanto el **codigo fuente del sistema**, como la **documentacion academica**, los **recursos de validacion** y las **metricas experimentales** del proyecto.

---

## Resumen del proyecto

El objetivo de este TFG es ampliar las capacidades de OBS Studio mediante un filtro nativo capaz de:

- detectar marcadores **ArUco** en tiempo real,
- estimar su pose con **OpenCV**,
- renderizar contenido virtual directamente dentro del pipeline de OBS,
- sincronizar informacion externa desde **DOMjudge**,
- y superponer elementos visuales utiles para retransmisiones tecnicas y eventos en vivo.

El resultado es una solucion pensada para contextos como **concursos de programacion**, demostraciones tecnologicas, emisiones educativas o entornos donde se necesite enriquecer una retransmision con informacion visual dinamica.

---

## Funcionalidades principales

- **Modo 3D**: superposicion de modelos 3D anclados a marcadores fisicos.
- **Modo realidad aumentada**: alineacion espacial del contenido virtual respecto a la escena real.
- **Modo countdown**: reloj de cuenta atras sincronizable con el tiempo oficial del concurso.
- **Modo scoreboard**: proyeccion en directo de la clasificacion del concurso.
- **Modo team info**: deteccion del equipo enfocado por camara y visualizacion de su informacion.
- **Sincronizacion web**: consulta periodica a la API REST de DOMjudge mediante `libcurl`.
- **Calibracion de camara**: herramienta auxiliar en Python para obtener parametros intrinsecos y corregir distorsion.
- **Integracion nativa en OBS**: sin procesos externos intermedios y con control desde la interfaz del propio software.

---

## Tecnologias utilizadas

- **C / C++**
- **OBS Studio SDK**
- **OpenCV**
- **Assimp**
- **libcurl**
- **CMake**
- **Python** para la calibracion de camara
- **LaTeX** para la memoria del TFG

---

## Arquitectura general

El sistema esta organizado en varios modulos especializados:

- `src/main_filter.c`: nucleo del filtro e integracion con OBS Studio.
- `src/aruco_detector.cpp`: deteccion de marcadores ArUco y estimacion de pose.
- `src/SJ_3DModel.c`: carga y renderizado de modelos 3D.
- `src/web_sync.c`: sincronizacion asincrona con la API de DOMjudge.
- `src/countdown_clock.c`: logica del reloj de cuenta atras.
- `src/json_utils.c`: utilidades de parseo JSON.
- `data/calibracion/calibrate.py`: script de calibracion de camara.

La arquitectura separa el procesamiento visual, el renderizado y la conectividad web para mantener una ejecucion estable dentro del ciclo de render de OBS.

---

## Estructura del repositorio

```text
TFG/
├── build_windows.bat
├── Release/
└── obs-domjudge-ar-overlay/
    ├── CMakeLists.txt
    ├── src/
    ├── data/
    ├── Dependencies/
    └── Sheila_Jose_TFG/
```

### Contenido destacado

- `obs-domjudge-ar-overlay/src/`: codigo fuente principal del plugin.
- `obs-domjudge-ar-overlay/data/`: recursos auxiliares, calibracion y mapeos.
- `obs-domjudge-ar-overlay/Dependencies/`: dependencias externas del proyecto.
- `obs-domjudge-ar-overlay/Sheila_Jose_TFG/`: memoria del TFG en LaTeX y PDF.
- `Release/`: binarios generados del plugin y librerias dinamicas necesarias para su ejecucion en Windows.

---

## Requisitos

Para trabajar con el proyecto se recomienda disponer de:

- **Windows x64**
- **CMake 3.16 o superior**
- **Visual Studio / MSVC**
- **OBS Studio 31.0.0**
- Dependencias incluidas o configuradas para:
  - **OpenCV 4.6**
  - **Assimp**
  - **libcurl**

> Nota: el desarrollo y la validacion del plugin se han realizado principalmente en entorno **Windows**.
> Nota de compatibilidad: el plugin ha sido desarrollado y validado especificamente para **OBS Studio 31.1.12** en **Windows x64**. No se garantiza su funcionamiento correcto en otras versiones principales de OBS Studio.

---

## Compilacion

El repositorio incluye un script de compilacion para Windows:

```bat
build_windows.bat
```

Este script ejecuta el siguiente flujo:

1. Configura el proyecto con `cmake --preset windows-x64`.
2. Compila las dependencias necesarias.
3. Genera las versiones `Debug`, `Release` y `RelWithDebInfo`.

Tambien puede ejecutarse manualmente:

```bat
cd obs-domjudge-ar-overlay
cmake --preset windows-x64
cmake --build --preset windows-x64 --target deps
cmake --build --preset windows-x64 --config Release
```

### Carpeta `Release`

La carpeta `Release/` recoge una compilacion lista para pruebas o integracion, incluyendo:

- el binario principal del plugin, como `obs-domjudge-ar-overlay.dll`,
- bibliotecas necesarias para vision artificial, como `opencv_core460.dll`, `opencv_aruco460.dll`, `opencv_imgproc460.dll` y `opencv_calib3d460.dll`,
- dependencias de carga de modelos 3D, como `assimp-vc143-mt.dll`.

Esta carpeta es el resultado final de compilacion y como apoyo para validar que el sistema genera correctamente los artefactos necesarios para su ejecucion.

---

## Flujo de uso

De forma resumida, el funcionamiento del sistema sigue este proceso:

1. Se configura una escena en OBS Studio con una fuente de video.
2. Se aplica el filtro del plugin sobre la fuente deseada.
3. El sistema detecta marcadores ArUco en cada fotograma en el modo correspondiente AR.
4. Se estima la pose del marcador respecto a la camara.
5. Se renderiza el contenido virtual o informativo sobre la imagen.
6. Si procede en el modo, se consulta la API de DOMjudge para actualizar tiempos, clasificacion o datos de equipo.

---

## Calibracion de camara

Para obtener una superposicion precisa, el sistema permite calibrar la camara con un script auxiliar:

```bash
python calibrate.py <calib_dir> <square_size> <output_file>
```

Ejemplo:

```bash
python calibrate.py calib_images 0.025 calibration.yml
```

La calibracion genera un fichero YAML con la matriz de camara y los coeficientes de distorsion utilizados por el detector.

---

## Integracion con DOMjudge

El plugin esta preparado para consultar informacion de concursos desde la API REST de **DOMjudge**, incluyendo:

- estado temporal del concurso,
- tiempo restante,
- clasificacion,
- informacion de equipos.

Ademas, el repositorio incorpora un ejemplo de mapeo entre marcadores ArUco y equipos:

- `obs-domjudge-ar-overlay/data/domjudge/marker_mappings.json`

Esto permite asociar un marcador fisico a un `team_id` concreto para el modo **Team Info**.

---

## Validacion y rendimiento

La validacion del sistema se apoya en:

- demostraciones funcionales de los distintos modos de operacion,
- mediciones de CPU, memoria y fotogramas por segundo,
- comparativas con multiples filtros activos,
- analisis del impacto del procesamiento ArUco en tiempo real.

Las evidencias experimentales del proyecto se encuentran en la documentacion

---

## Documentacion academica

La memoria del Trabajo de Fin de Grado se encuentra en:

- [Memoria completa en PDF](/Users/sheila/Desktop/TFG/obs-plugintemplate-master/Sheila_Jose_TFG/TFGTeXiS.pdf)
- [Fuente principal en LaTeX](/Users/sheila/Desktop/TFG/obs-plugintemplate-master/Sheila_Jose_TFG/TFGTeXiS.tex)

En esta documentacion se desarrolla el contexto teorico, la arquitectura, la implementacion, la metodologia de validacion y las conclusiones del proyecto.

---

## Aportacion del proyecto

Este trabajo demuestra la viabilidad de integrar un sistema de **realidad aumentada orientado a datos** dentro de OBS Studio manteniendo una arquitectura nativa, modular y util para escenarios reales de retransmision.

La propuesta no solo resuelve un problema tecnico, sino que abre una linea de aplicacion clara en:

- produccion audiovisual,
- competiciones de programacion,
- divulgacion tecnologica,
- docencia,
- y eventos en directo con soporte visual enriquecido.

---

## Estado del proyecto

El proyecto se encuentra en un estado **funcional y validado experimentalmente** como prototipo academico de TFG.

Entre las posibles lineas de mejora destacan:

- compatibilidad con otros sistemas de gestion de concursos,
- optimizacion del coste computacional con multiples filtros AR,
- soporte multiplataforma

---

## Autoria

Proyecto desarrollado en el contexto de un **Trabajo de Fin de Grado**.

Autores indicados en la configuracion del proyecto:

- **Sheila Julvez**
- **Jose Moreno**

---

## Licencia

Este repositorio incluye licencia en el archivo:

- `obs-domjudge-ar-overlay/LICENSE`

Se recomienda revisar sus terminos antes de redistribuir o reutilizar el codigo.
