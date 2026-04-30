# Arquitectura de Datos Dinámicos y Scoreboard (Marzo 2025)

Esta documentación describe el sistema completo de sincronización, procesamiento y renderizado de datos dinámicos implementado en el plugin.

## 1. Pipeline de Datos Dinámicos (C & libcurl)

El plugin utiliza un hilo secundario independiente para evitar bloqueos en el hilo de renderizado de OBS.

### Sincronización Web (`web_sync.c`)
- **Hilo de Red**: Se crea un hilo mediante `CreateThread` que ejecuta un bucle infinito controlado por `sync_thread_func`.
- **Motor Curl**: Utiliza `libcurl` con un callback de memoria (`write_callback`) para capturar las respuestas JSON del servidor DOMjudge.
- **Parser de JSON Nativo**: Para minimizar dependencias, se ha desarrollado un parser manual de JSON que busca claves (`parse_json_string`, `parse_json_int`, `parse_json_int_nested`) de forma eficiente.
- **Sincronización Temporal**: El plugin extrae la cabecera `Date` del servidor HTTP para sincronizar el reloj interno con el del concurso, evitando discrepancias milimétricas entre el PC y el servidor.

### Transferencia de Datos Thread-Safe
- **Critical Sections**: Se protegen los buffers de resultados con `CRITICAL_SECTION`. 
- **Mecanismo de Polling**: Cada frame (`filter_tick`), el plugin principal llama a `web_sync_poll`. Si hay datos nuevos, se actualiza la estructura `scoreboard_teams` del filtro.

## 2. Sistema de Overlay y Renderizado

El renderizado del texto no se hace de forma estática, sino integrada en el pipeline de OBS.

### Rendimiento y Fuentes Privadas
- **Fuentes Privadas**: El plugin crea una fuente interna de tipo `text_gdiplus_v2` (GDI+) de forma privada. Esto permite que el scoreboard sea parte del filtro sin necesitar que el usuario cree una fuente de texto manual en su escena.
- **Actualización Dinámica**: Cuando cambian los datos (ej. un equipo resuelve un problema), se regenera el string del scoreboard y se inyecta en la fuente privada mediante `obs_source_update`.

### pipeline de Renderizado (`filter_render`)
- **Proyección Ortográfica**: Se configura una matriz `gs_ortho` de 2D sobre el canvas de OBS.
- **Cálculo de Posición Inteligente**:
  - **Centrado automático**: Calcula `(ScreenWidth - TextWidth) / 2`.
  - **Posicionamiento Manual (Modo Superior-Superior)**: Se ha implementado un cálculo que permite usar coordenadas naturales (0 es arriba). Internamente se traduce a la posición de OpenGL: `final_y = height_screen - offset_y - text_height`.
- **Z-Order**: El overlay se dibuja después del modelo 3D pero antes de los efectos de post-procesado.

## 3. Entorno de Desarrollo: Mock Server (Python)

Para permitir el desarrollo sin conexión a un servidor DOMjudge real, se ha creado `mock_server.py`.

### Características del Servidor de Pruebas
- **Flask Framework**: Un servidor ligero en Python que escucha en el puerto 5000.
- **Simulación Realista de Endpoints**:
  - `/api/v4/contests/{cid}`: Información de tiempos y duración.
  - `/api/v4/contests/{cid}/scoreboard`: Datos de ranking, problemas resueltos y tiempo.
  - `/api/v4/contests/{cid}/teams`: Nombres reales de equipos (usado para la caché de nombres).
- **Dinamismo**: El servidor de pruebas genera cambios automáticos en el scoreboard según el tiempo transcurrido desde su inicio, permitiendo probar la actualización en tiempo real de la UI en OBS sin intervención manual.

## 4. Resumen de Controles en OBS

El usuario final ahora dispone de:
1. **Sincronización con DOMjudge**: Interruptor para habilitar/deshabilitar la red.
2. **Intervalo Personalizable**: Frecuencia de actualización desde 1 segundo en adelante.
3. **Offset X/Y**: Posicionamiento pixel-perfect del texto.
4. **Tamaño de Fuente**: Control independiente del escalado del objeto 3D.

---
