# Librerías usadas para Countdown y Web Sync

Para el reloj de cuenta atrás y la sincronización web se usan **dos librerías** además de las que ya tenía el plugin (OBS, Assimp, OpenCV, Qt):

| Librería   | Uso en el plugin        | Enlace en CMake actual |
|-----------|--------------------------|-------------------------|
| **libcurl** | Peticiones HTTP a la API REST (en un hilo aparte) | `CURL::libcurl` |
| **pthread (w32-pthreads)** | Hilo secundario en `web_sync.c` | En Windows: **w32-pthreads de OBS** (includes + `pthreadVC3.lib` / `pthreadVC3d.lib`). En Linux/macOS: pthread del sistema. |

---

## 1. libcurl

- **Qué hace:** peticiones HTTP GET a la URL que devuelve el JSON con `hours`, `minutes`, `seconds`.
- **Includes:** `#include <curl/curl.h>` (y `#include <pthread.h>` en `web_sync.c`).
- **En CMake:** `find_package(CURL REQUIRED)` y `target_link_libraries(..., CURL::libcurl)`.  
  CMake se encarga de los **includes** y de enlazar la **lib** de curl.

### Dónde descargar / instalar

**Windows (Visual Studio):**

- **Opción A – vcpkg (recomendada)**  
  1. Instala [vcpkg](https://vcpkg.io/en/docs/README.html).  
  2. En una terminal (ej. desde la raíz del repo):
     ```bash
     vcpkg install curl:x64-windows
     ```
  3. Configura CMake con vcpkg (sustituye `<ruta-vcpkg>` por tu instalación):
     ```bash
     cmake -B build -DCMAKE_TOOLCHAIN_FILE=<ruta-vcpkg>/scripts/buildsystems/vcpkg.cmake
     ```
     Así CMake encuentra CURL y sus includes/libs automáticamente.

- **Opción B – Binarios precompilados**  
  - Descarga builds de [curl-for-win](https://github.com/curl/curl-for-win/releases) o [curl.se – Windows](https://curl.se/windows/).  
  - Descomprime en una carpeta, por ejemplo `Dependencies/curl` o `.deps/curl`.  
  - En `CMakeLists.txt` (antes de `find_package(CURL REQUIRED)`), indica dónde está curl:
    ```cmake
    set(CURL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/curl")  # o la ruta que uses
    set(CMAKE_PREFIX_PATH "${CURL_ROOT};${CMAKE_PREFIX_PATH}")
    ```
    O bien:
    ```cmake
    set(CURL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/curl/cmake")  # si el paquete trae CMake config
    ```
  - La estructura típica del paquete es algo como:
    - `include/curl/`  → cabeceras  
    - `lib/`           → `.lib` (y en runtime las DLL si las usa)

**Linux:**

```bash
# Debian/Ubuntu
sudo apt install libcurl4-openssl-dev

# Fedora
sudo dnf install libcurl-devel
```

CMake con `find_package(CURL REQUIRED)` suele encontrarlas sin rutas extra.

**macOS:**

```bash
brew install curl
```

Normalmente no hace falta añadir rutas si usas el curl de Homebrew y CMake lo toma por defecto.

---

## 2. pthread (w32-pthreads de OBS en Windows)

- **Qué hace:** `pthread_create`, `pthread_mutex_lock`/`unlock`, etc. en `web_sync.c`.
- **Importante (Windows):** `find_package(Threads REQUIRED)` no enlaza pthread; hay que enlazar explícitamente w32-pthreads de OBS (includes + pthreadVC3.lib / pthreadVC3d.lib).

En el proyecto se buscan includes y libs en **Dependencies/w32-pthreads**:

```cmake
set(w32-pthreads_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/w32-pthreads")
include_directories("${w32-pthreads_DIR}/include")
```

Y en `target_link_libraries`:

```cmake
# w32-pthreads de OBS (compatibilidad binaria con OBS Studio en Windows)
$<$<CONFIG:Debug>:${w32-pthreads_DIR}/lib/pthreadVC3d.lib>
$<$<CONFIG:Release>:${w32-pthreads_DIR}/lib/pthreadVC3.lib>
```

**No** se usa `Threads::Threads` aquí; en Windows no aporta pthread.

Estructura esperada en **Dependencies/w32-pthreads**:
- `include/` — cabeceras (p. ej. `pthread.h`)
- `lib/` — `pthreadVC3.lib` (Release) y `pthreadVC3d.lib` (Debug)

Puedes copiar la carpeta desde `.deps/obs-studio-.../deps/w32-pthreads` o descargar [w32-pthreads](https://sourceforge.net/projects/pthreads4w/) y descomprimir ahí. Si los `.lib` se llaman distinto (p. ej. `pthreadVC2.lib`), ajusta el nombre en el CMake manteniendo Debug/Release.

### Justificación para el TFG

> En Windows, OBS Studio no utiliza la API nativa de hilos, sino una implementación POSIX (w32-pthreads) incluida como dependencia del proyecto. Para garantizar compatibilidad binaria y estabilidad, el plugin enlaza explícitamente contra la versión de pthread distribuida con OBS Studio.

### Opción alternativa – vcpkg (si no usas deps de OBS)

```bash
vcpkg install pthreads:x64-windows
```

Y configurar CMake con el toolchain de vcpkg (igual que para curl). Luego enlazar el target que vcpkg te indique (suele ser algo como `pthreads` o `PThreads::PThreads` si tiene módulo CMake).

### Opción C – Descargar w32-pthreads a mano

- Fuente: [GitHub – w32-pthreads (sourceforge)](https://github.com/nicola-gigante/w32-pthreads) o el [SourceForge original](https://sourceforge.net/projects/pthreads4w/).  
- Descomprime en algo como `Dependencies/pthreads-w32`.  
- En `CMakeLists.txt`:
  - `include_directories(Dependencies/pthreads-w32/include)` (o la carpeta donde esté `pthread.h`).
  - `link_directories(Dependencies/pthreads-w32/lib/x64)` (o la carpeta con el `.lib`).
  - `target_link_libraries(${_name} PRIVATE pthreadVC3)` (o el nombre del .lib que tengas).

---

## Resumen rápido para tu máquina (Windows)

1. **libcurl**  
   - Instalar con vcpkg: `vcpkg install curl:x64-windows` y usar `-DCMAKE_TOOLCHAIN_FILE=...` al configurar CMake, **o**  
   - Descargar precompilados de curl, ponerlos en una carpeta (ej. `Dependencies/curl`) y setear `CURL_ROOT` o `CMAKE_PREFIX_PATH` como arriba.

2. **pthread**  
   - Se usan **w32-pthreads** desde **Dependencies/w32-pthreads** (incluye `include/` y `lib/` con `pthreadVC3.lib` / `pthreadVC3d.lib`). No se usa `Threads::Threads`.

3. **En CMake**  
   - Includes: `find_package(CURL)` para curl + `include_directories("${w32-pthreads_DIR}/include")` para pthread.  
   - Libs: `CURL::libcurl` + las dos líneas condicionales Debug/Release con `pthreadVC3d.lib` y `pthreadVC3.lib`.
