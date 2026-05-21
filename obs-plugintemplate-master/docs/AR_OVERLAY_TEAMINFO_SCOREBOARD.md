# Overlay AR (Scoreboard/Team Info) y JSON local: documentacion tecnica

Este documento describe, a nivel tecnico y **funcion a funcion**, los cambios implementados para:

- Cargar un JSON local en el modo **Team Info** (mapeo `marker_id -> team_id`).
- Detectar **multiples** marcadores ArUco definidos por ese JSON (en una sola pasada).
- Alinear el overlay (modo **3 Scoreboard** y modo **4 Team Info**) al marcador (posicion/rotacion/escala).
- Mejorar la estetica del overlay con **fondo**, **esquinas redondeadas** y **sombra**.
- Reducir el jitter con **suavizado AR** (EMA) en posicion/escala/angulo del overlay.

> Nota: todo el render se hace con APIs nativas de OBS/libobs (`gs_*` + `obs_get_base_effect`) y geometria generada a mano.
> No hay dependencias extra de UI mas alla de `obs_properties_*`.

---

## 1) Archivo: `src/aruco_detector.h`

### `process_frame_rgba(...)`
- **Estado**: se mantiene para compatibilidad.
- **Funcion**: detecta marcadores con OpenCV, pero selecciona el marcador cuyo ID coincide con `det->id`.
- **Motivo**: otros modos del plugin siguen usando un ID fijo.

### `process_frame_rgba_select_ids(...)` (nuevo)
- **Motivo**: en Team Info el JSON define varios IDs validos. No se puede “forzar” `det->id=0` o iterar N veces (seria caro).
- **Contrato**:
  - Hace **una** `detectMarkers(...)`.
  - Elige un marcador cuyo ID este dentro de `allowed_ids[]`.
  - Rellena `ArucoResult` (corners, centro en pantalla, rvec/tvec y angulo).
- **No bloquea render**: se llama en el camino de video (`filter_video`), no en `filter_render`.

---

## 2) Archivo: `src/aruco_detector.cpp`

### `id_permitido(int id, const int *allowed_ids, size_t allowed_count)` (nuevo)
- **Funcion**: comprueba si un ID existe en la lista permitida.
- **Uso**: filtrar resultados de `detectMarkers`.

### `area_cuadrilatero(const std::vector<cv::Point2f> &c)` (nuevo)
- **Funcion**: calcula un area 2D (shoelace) para estimar “marcador mas grande en pantalla”.
- **Uso**: si aparecen varios marcadores permitidos, se elige el de mayor area para estabilizar el overlay (normalmente el mas cercano/visible).

### `process_frame_rgba_select_ids(...)` (nuevo)
Pipeline interno:
1. Convierte el frame OBS a BGRA (`obs_frame_to_bgra`).
2. Convierte a gris y ejecuta `cv::aruco::detectMarkers`.
3. Filtra IDs usando `id_permitido`.
4. Selecciona el mejor candidato por area (`area_cuadrilatero`).
5. Estima pose con `estimatePoseSingleMarkers` y copia `rvec/tvec`.
6. Convierte corners a coordenadas “base” (resolucion base de OBS) y calcula:
   - `res->screen_pos_x/y` como el centro (promedio de 4 esquinas).
   - `res->corners[4][2]` en el mismo espacio.
7. Calcula Euler para trazas (`rotation_to_euler`) y deja `res->euler_*`.

---

## 3) Archivo: `src/main_filter.c`

### 3.1 Estructuras y memoria (datos persistentes del filtro)

#### `struct cube_filter_data` (campos relevantes)
- **JSON Team Info**:
  - `team_info_json_path`: ruta persistida en settings.
  - `team_info_mappings` + `team_info_mappings_count`: array dinamico con mapeos `aruco_id -> team_id`.
  - `team_info_allowed_marker_ids` + `team_info_allowed_marker_ids_count`: lista “compacta” de IDs permitidos derivada del JSON (evita recomputar por frame).
- **Overlay**:
  - `overlay_bg_*`: parametros del fondo (color/opacidad/padding/radio/sombra).
  - `overlay_bg_vb`: quad unidad (0..1) precargado en GPU.
  - `overlay_bg_round_vb`: geometria triangulada en pixeles para rectangulo redondeado (se regenera cuando cambia w/h/radio).
- **Suavizado AR**:
  - `overlay_ar_smooth_*`: estado y parametros EMA.

#### `team_info_clear_mappings(filter)`
- **Funcion**: libera memoria del JSON en el filtro.
- Libera:
  - `team_info_mappings`
  - `team_info_allowed_marker_ids`
- **Objetivo**: no acumular memoria al recargar JSON o cambiar ruta.

#### `team_info_load_json_from_path(filter, path)`
- **Funcion**: lee el fichero y parsea mappings.
- **Despues del parseo**: construye `team_info_allowed_marker_ids[]` a partir de `team_info_mappings[]` eliminando duplicados.

### 3.2 Deteccion (no render)

#### `filter_video(data, frame)`
- **Objetivo**: ejecutar deteccion ArUco fuera del hilo de render y preparar `last_result`.
- **Cambio clave**: para alinear bien el overlay 2D, el detector se llama usando el mismo espacio de coordenadas que el render 2D:
  - `base_w/base_h = ovi.base_width/base_height`
  - `out_w/out_h = base_w/base_h`
- **Modo 4 (Team Info)**:
  - Si existe lista `team_info_allowed_marker_ids_count > 0`, se usa `process_frame_rgba_select_ids(...)`.
- **Otros modos**:
  - Se usa `process_frame_rgba(...)` (compatibilidad con ID fijo).

### 3.3 Overlay: alineacion, fondo, sombra, suavizado

#### `aruco_marker_metrics_2d(res, out_edge_px, out_angle_rad)`
- **Funcion**: calcula (1) lado medio del marcador en pixeles y (2) angulo en pantalla a partir de corners.
- **Uso**: escala del overlay y rotacion 2D.

#### `overlay_bg_init_graphics(filter)`
- **Funcion**: crea un quad unidad en GPU con `gs_render_start` / `gs_render_save`.
- **Uso**: fallback rapido cuando no se usan esquinas redondeadas.

#### `overlay_bg_update_rounded_geometry(filter, w, h, radius_px)` (nuevo)
- **Funcion**: genera un poligono convexo con esquinas redondeadas y lo triangula como fan.
- **Resultado**: `overlay_bg_round_vb` en GPU con vertices ya en **pixeles**.
- **Regeneracion**:
  - Solo si cambia `w/h/radius` de forma apreciable.
  - Si `radius_px <= 0`, se destruye `overlay_bg_round_vb`.

#### `overlay_bg_free_graphics(filter)` / `overlay_bg_free_round_graphics(filter)`
- **Funcion**: destruye vertexbuffers del fondo.
- **Objetivo**: evitar leaks de recursos GPU al destruir el filtro.

#### `wrap_angle_delta(current, target)` (nuevo)
- **Funcion**: calcula el delta angular mas corto ([-pi, pi]).
- **Uso**: suavizado EMA del angulo sin “saltos” al cruzar ±pi.

#### `filter_render(data, effect)`
Dentro del bloque de overlay (modo 3/4):
1. Calcula `final_x/final_y`:
   - Si hay marcador: `last_result.screen_pos_x/y + offsets`.
2. Si hay marcador:
   - Calcula `overlay_scale` segun lado del marcador (`marker_edge_px / max(text_w, text_h)`).
   - Calcula `angle_screen`.
   - **Suavizado** (si activado):
     - EMA para `x/y/scale/angle` con `overlay_ar_smooth_alpha`.
3. Aplica transformaciones `gs_matrix_*` en este orden:
   - translate al centro del marcador
   - (opcional) correccion por suavizado
   - rotacion por `angle_screen`
   - escala por `overlay_scale`
   - translate `-tw/2, -th/2` para centrar el texto en el marcador
4. Dibuja sombra + fondo (antes del texto) con `OBS_EFFECT_SOLID`.
5. Renderiza el texto con `obs_source_video_render(scoreboard_text_source)`.

### 3.4 UI / settings (persistencia)

#### `filter_properties(...)`
Se añadieron propiedades nuevas (modo 3 y 4):
- `overlay_bg_enabled`, `overlay_bg_color`, `overlay_bg_opacity`, `overlay_bg_padding`
- `overlay_bg_radius`
- `overlay_bg_shadow_enabled`, `overlay_bg_shadow_opacity`, `overlay_bg_shadow_offset_x/y`, `overlay_bg_shadow_softness`
- `overlay_ar_smooth_enabled`, `overlay_ar_smooth_alpha`

#### `render_mode_changed(...)`
- Se controla visibilidad de estas propiedades solo en Scoreboard/Team Info.

#### `filter_update(...)`, `filter_save(...)`, `filter_defaults(...)`
- Se leen/guardan defaults de todos los parametros del fondo/sombra y suavizado.

### 3.5 Liberacion de recursos (leaks)

#### `filter_destroy(data)`
Liberaciones relevantes:
- `cleanup_global_meshes(...)` (mallas/recursos 3D)
- `overlay_bg_free_graphics(...)` (vertexbuffers del fondo)
- `cleanup_aruco_detector(...)` (**importante**: evita fuga del detector/OpenCV)
- `team_info_clear_mappings(...)` (JSON Team Info)
- Texturas y zstencil (`gs_texture_destroy`, `gs_zstencil_destroy`)

---

## 4) Archivo: `src/web_sync.c`

### `parse_teams_json(...)` (ajuste de seguridad)
- **Cambio**: asegurar terminador `'\0'` en `team_id` y `team_name` despues de `strncpy`.
- **Motivo**: evitar lecturas fuera de buffer que producian texto corrupto en el overlay.

---

## 5) Archivo: `CMakeLists.txt`

### `target_compile_options(... /utf-8)` (MSVC)
- **Motivo**: forzar UTF-8 en literales de strings en compilacion MSVC.
- **Resultado**: menos problemas de codificacion en UI/logs si se vuelven a introducir caracteres no ASCII.

---

## 6) JSON de ejemplo

Archivo: `data/team_info_mappings.example.json`

Formato recomendado (array de objetos):
```json
[
  { "marker_id": 1, "team_id": "42" },
  { "marker_id": 2, "team_id": "12" }
]
```

---

## 7) Limitaciones conocidas / recomendaciones

- **Sombra**: es una aproximacion por capas (varios draws). Si se quiere blur real, lo ideal es:
  - render a una textura y aplicar un shader de blur, o
  - usar un efecto custom con sampling.
- **Rounded geometry**: se regenera cuando cambia el tamano del texto. Si el texto cambia cada frame, puede regenerar a menudo.
  - Mitigacion: cache por tamaños discretos o regenerar con hysteresis mayor.
- **Suavizado**: EMA funciona bien para jitter. Para tracking mas “fisico”, se puede usar filtro de Kalman 2D.
