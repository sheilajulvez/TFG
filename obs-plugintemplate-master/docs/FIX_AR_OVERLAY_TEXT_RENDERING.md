# Fix del renderizado del texto AR en modos 3 (Scoreboard) y 4 (Team Info)

> **Estado:** Aplicado y verificado en runtime para el clipping. Pendiente de prueba final del swap Y↔Z.
> **Archivo modificado:** `src/main_filter.c` (un solo archivo, dos bloques quirúrgicos).
> **Sin tocar:** `aruco_marker_metrics_2d`, `process_frame_rgba*`, suavizado AR, escalado y posicionamiento del marcador.

---

## 1. Síntomas reportados

1. **Clipping del texto.** En los modos 3 y 4, cuando el texto del overlay era grande, partes del mismo desaparecían como si saliesen por el plano de corte cercano (near) o lejano (far). Ajustar los valores de `znear`/`zfar` en `gs_set_3d_mode(...)` no producía ningún efecto, por más extremos que fueran.
2. **Rotaciones cruzadas.** Los offsets manuales `ar_offset_rot_x/y/z` se comportaban de forma no intuitiva: arreglar la rotación de un eje rompía el comportamiento de otro. Concretamente, los ejes Y y Z del usuario aparecían intercambiados respecto a lo esperado, y la rotación X interactuaba con la pose del marcador.

---

## 2. Diagnóstico técnico

### 2.1 Por qué los `znear`/`zfar` no tenían efecto

`gs_set_3d_mode(...)` en libobs **es un stub no implementado**. Comprobado leyendo el código fuente de OBS Studio 31.0.0:

```c
// .deps/obs-studio-31.0.0/libobs/graphics/graphics.c, línea 1078
void gs_set_3d_mode(double fovy, double znear, double zvar)
{
    /* TODO */
    UNUSED_PARAMETER(fovy);
    UNUSED_PARAMETER(znear);
    UNUSED_PARAMETER(zvar);
}
```

Los tres parámetros se descartan inmediatamente. Por tanto, las llamadas

```c
// main_filter.c (estado anterior)
gs_set_3d_mode(60.0f, 0.000001f, 5000.0f);   // modos 3/4
gs_set_3d_mode(60.0f, 0.1f,      5000.0f);   // modo 0/1/2 (3D)
```

no configuraban ninguna proyección. La que estaba activa cuando se renderizaba el overlay era la que OBS pone por defecto al filtro: una `gs_ortho` con un rango de Z muy reducido y sesgado a un lado. La definición canónica de OBS para el render de filtros es esencialmente:

```c
gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -1.0f, -1024.0f);
```

Es decir, **el rango Z visible es solo 1023 unidades**, todo en el lado negativo. Cuando en modos 3/4 se levanta el panel de texto con `gs_matrix_rotaa4f(1, 0, 0, π/2)`, el texto pasa de estar plano en el plano XY a extenderse en el eje Z. Un texto de 200 px de alto centrado en Z=0 ocupa de Z=−100 a Z=+100. La parte con Z positivo cae fuera del rango visible y la GPU la recorta. Cuanto mayor el texto, mayor la franja recortada — exactamente el síntoma observado.

### 2.2 Por qué las rotaciones manuales se cruzaban entre ejes

OBS, igual que OpenGL, usa un *stack* de matrices y las funciones `gs_matrix_rotaa4f`, `gs_matrix_translate3f`, etc. **post-multiplican** sobre la matriz actual:

```
M_new = M_old · R
```

Esto significa que cuando se renderiza un vértice `v`:

```
v_world = M_final · v
        = (T_pos · R₁ · R₂ · ... · Rₙ · T_center) · v
```

la matriz aplicada **primero** al vértice es la que aparece **última** en el código (la más cercana a `T_center`).

El bloque del overlay 3D, en su versión anterior, hacía (orden de código):

```c
// 1. Translate al centro del marcador
gs_matrix_translate3f(overlay_pos_x, overlay_pos_y, overlay_pos_z);
// 2. Corrección OpenCV → OBS
gs_matrix_rotaa4f(1, 0, 0, M_PI);                    // R_x(180)
// 3. Rotación de la pose del marcador
gs_matrix_rotaa4f(ax, ay, -az, rangle);              // R_aruco
// 4. "Levantar" el panel de tendido a vertical
gs_matrix_rotaa4f(1, 0, 0, π/2);                     // R_x(90)
// 5. Offsets manuales del usuario (ESTABAN AQUI)
gs_matrix_rotaa4f(1, 0, 0, ar_offset_rot_x);
gs_matrix_rotaa4f(0, 1, 0, ar_offset_rot_y);
gs_matrix_rotaa4f(0, 0, 1, ar_offset_rot_z);
// 6. Centrar el texto en su propio centro
gs_matrix_translate3f(-tw/2, -th, 0);                // T_center
```

Sobre el vértice, esto se aplica de derecha a izquierda:

1. `T_center`: el texto queda centrado en el origen local.
2. `R_offset_z` luego `R_offset_y` luego `R_offset_x`: rotaciones en el frame del **panel sin levantar**, plano sobre XY.
3. `R_x(90)`: levanta el panel. **Esta rotación reordena los ejes locales del panel respecto al mundo**: el eje Y del panel pasa a apuntar a `+Z` del mundo, y el eje Z del panel pasa a `-Y` del mundo.
4. `R_aruco`: orienta según la pose del marcador.
5. `R_x(180)`: corrección de coordenadas.
6. `T_pos`: traslada a la posición en pantalla.

Resultado: cuando el usuario movía el slider Y esperando un *yaw* (giro tipo puerta sobre el eje vertical), el `R_x(90)` posterior reorientaba ese eje a profundidad y lo que veía era un *roll*. Análogo cruce con el eje Z. Y cualquier rotación del marcador (`R_aruco`) mezclaba los tres ejes adicionalmente, por lo que ajustar un signo solo era válido para una orientación específica del marcador.

### 2.3 Sistemas de coordenadas en juego

| Sistema | X | Y | Z | Origen |
|---|---|---|---|---|
| **OpenCV** (cámara) | derecha | abajo | hacia la escena (saliendo de la cámara) | esquina sup-izq de la imagen |
| **OBS filtro** (gs_ortho 0..W, 0..H, −1..−1024) | derecha | **abajo** (Y-Down) | hacia el espectador (rango negativo en eye-space) | esquina sup-izq de la fuente |
| **Marcador** (rvec/tvec ArUco) | borde 1 del marcador | borde 2 del marcador | normal (sale de la cara impresa) | centro del marcador |

X y Y coinciden entre OpenCV y OBS (Y-Down en ambos). Z difiere en sentido. Por eso la conversión `R_x(180)` + negar `rvec[2]` es una combinación habitual y correcta: voltea Z y mantiene la orientación de X/Y.

---

## 3. Soluciones aplicadas

### 3.1 Cambio 1 — Sustituir `gs_set_3d_mode` por `gs_ortho` con rango Z amplio

**Ubicación:** `src/main_filter.c`, dentro de `filter_render`, en el bloque del overlay (≈ línea 1686-1704).

**Antes:**

```c
gs_projection_push();
const bool overlay_use_3d_pose = align_overlay_to_aruco && filter->last_result.detected;
if (overlay_use_3d_pose) {
    gs_set_3d_mode(60.0f, 0.000001f, 5000.0f);   // NO-OP
} else {
    gs_ortho(0.0f, (float)current_source_w, 0.0f, (float)current_source_h, -100.0f, 100.0f);
}
```

**Después:**

```c
gs_projection_push();
const bool overlay_use_3d_pose = align_overlay_to_aruco && filter->last_result.detected;
if (overlay_use_3d_pose) {
    /* gs_set_3d_mode(...) en libobs es un stub no implementado.
     * Usamos gs_ortho con un rango Z amplio y simétrico: las coordenadas
     * X/Y siguen siendo píxeles (no rompe el posicionamiento del marcador)
     * y la profundidad da margen para cualquier rotación del panel. */
    gs_ortho(0.0f, (float)current_source_w,
             0.0f, (float)current_source_h,
             -10000.0f, 10000.0f);
} else {
    gs_ortho(0.0f, (float)current_source_w, 0.0f, (float)current_source_h, -100.0f, 100.0f);
}
```

**Por qué funciona.** `gs_ortho` sí está implementada. El mapeo X/Y en píxeles se conserva (no rompe `gs_matrix_translate3f(final_x, final_y, …)` ni la métrica del marcador), y se amplía la profundidad visible a ±10 000 unidades, suficiente para textos arbitrariamente grandes y rotaciones extremas. **No** introduce perspectiva (foreshortening); para eso harían falta `gs_frustum` o cargar manualmente una matriz de perspectiva con `gs_matrix_set`.

**Estado del modo 0/1/2 (modelos 3D).** La llamada equivalente en `main_filter.c` línea 1484 sigue siendo `gs_set_3d_mode(60.0f, 0.1f, 5000.0f)`, también no-op. No se ha tocado porque actualmente no presenta el síntoma. Si en el futuro un modelo grande aparece recortado, aplicar el mismo patrón.

### 3.2 Cambio 2 — Reordenar los offsets al frame de pantalla

**Ubicación:** `src/main_filter.c`, dentro del bloque `if (overlay_use_3d_pose)` (≈ línea 1786-1832).

**Antes:** los offsets manuales estaban al final del bloque, después de `R_x(180)`, `R_aruco` y `R_x(90)`. Al post-multiplicar, esto los situaba sobre el vértice **antes** que esas correcciones, en el frame del panel sin levantar.

**Después:** los offsets se aplican **inmediatamente después del translate al marcador** y **antes** de cualquier rotación de coordenadas o pose. Mapeo directo de los sliders a los ejes mundiales (sin swap):

```c
gs_matrix_translate3f(overlay_pos_x, overlay_pos_y, overlay_pos_z);

/* Offsets manuales en frame de pantalla */
gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, ar_offset_rot_x * deg2rad);  // eje X mundial
gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, ar_offset_rot_y * deg2rad);  // eje Y mundial
gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, ar_offset_rot_z * deg2rad);  // eje Z mundial

/* A continuación R_x(180), R_aruco, R_x(90) como antes */
```

**Por qué funciona — el orden.** Con este orden, cuando los offsets se aplican al vértice, el panel ya ha pasado por `T_center`, `R_x(90)` (levantado), `R_aruco` (orientado al marcador) y `R_x(180)` (corrección OpenCV). Es decir, el panel está completamente orientado y centrado en el origen (el `T_pos` viene después en el código, antes en operación, así que aún no se ha aplicado al vértice). Por tanto los offsets:

- Rotan al **panel completo ya orientado**, no al texto plano sin levantar.
- Operan respecto a los **ejes mundiales** (los del frame de pantalla), no respecto a los ejes del panel local.
- Son **independientes de la orientación del marcador** — `R_aruco` ya está aplicada cuando los offsets entran en juego.

Como el panel está centrado en el origen en el momento del giro, las rotaciones siguen siendo respecto al centro del overlay, no respecto al origen de la pantalla.

**Limitación conocida — Y/Z percibidos como cruzados.** OBS proyecta con Y-Down y el usuario percibe el panel ya levantado por `R_x(90)`. En ese mapa mental, lo que el usuario llama "Y" coincide visualmente con el eje Z del mundo y viceversa, así que mover el slider Y produce un efecto que el usuario etiqueta como Z, y el slider Z produce un efecto que etiqueta como Y. **No se ha aplicado un swap en código** porque cualquier swap rompe la composición con la rotación del marcador para orientaciones no triviales. Solución limpia pendiente: renombrar las etiquetas de los sliders en `obs_properties_add_float_slider` (líneas ≈ 2308-2310 de `main_filter.c`) para que reflejen el comportamiento percibido.

Mapeo actual:

| Slider | Eje OBS | Comportamiento percibido por el usuario |
|---|---|---|
| `ar_offset_rot_x` | Pitch — el panel se inclina hacia delante/atrás | X (1,0,0) |
| `ar_offset_rot_y` | Yaw — el panel gira como una puerta sobre eje vertical | Z (0,0,1) |
| `ar_offset_rot_z` | Roll — el panel gira en su propio plano como rueda | Y (0,1,0) |

---

## 4. Cómo verificar visualmente

1. **Clipping (modos 3 y 4).** Poner una fuente grande (80–100 px) en el texto del scoreboard. Inclinar físicamente el marcador. El texto no debe recortarse por planos invisibles ni "comerse" por los lados.
2. **Pitch (slider X).** Marcador frontal y centrado. Mover `ar_offset_rot_x` de 0 a 30°. El panel debe inclinarse hacia atrás/adelante sin moverse en X ni en Y.
3. **Yaw (slider Y).** Mover `ar_offset_rot_y` de 0 a 30°. El panel debe rotar como una puerta sobre el eje vertical de la pantalla.
4. **Roll (slider Z).** Mover `ar_offset_rot_z` de 0 a 30°. El panel debe girar en su propio plano, como una rueda mirando al espectador.
5. **Independencia.** Repetir las pruebas 2-4 con el marcador inclinado físicamente. Los offsets deben seguir comportándose igual desde el punto de vista del espectador, no del marcador.

Si en alguno de los tres ejes la dirección de giro queda al revés, basta con negar el `* ((float)M_PI / 180.0f)` para ese eje (o anteponer un `-` al valor del slider).

---

## 5. Restricciones respetadas

- **No se ha tocado el "core" del posicionamiento.** `aruco_marker_metrics_2d` (cálculo del centro del marcador y dimensiones), `process_frame_rgba*` (pipeline de detección), el suavizado AR (EMA de posición/escala/ángulo) y la estimación de `final_x/final_y` siguen exactamente como estaban.
- **No se ha cambiado la rotación del modelo 3D.** El bloque `render_model_c` / `render_model_clock_c` no se ha modificado; siguen usando la convención previa.
- **No se han tocado los settings ni la UI.** `filter_properties`, `filter_update`, `filter_save`, `filter_load`, `filter_defaults` están intactos. Los rangos `-360..360` de los sliders y los nombres `ar_offset_rot_x/y/z` se conservan.

---

## 6. Compilación

Como tu instalación de OBS bloquea el DLL en Release, compilar siempre en **Debug** mientras se valida el cambio:

```powershell
cd C:\Users\josem\Desktop\jose\tfg\TFG\obs-plugintemplate-master
cmake --build build_x64 --config Debug
```

Y reiniciar OBS para recargar el plugin tras cada compilación.

---

## 7. Próximos pasos sugeridos (no aplicados, candidatos para mejoras futuras)

- **Perspectiva real** con `gs_frustum(...)` en lugar de `gs_ortho` ampliada, para que el panel inclinado en pitch/yaw muestre foreshortening realista.
- **Offsets en frame del marcador** como modo alternativo (opción C de la sesión de análisis): aplicar los offsets entre `R_aruco` y la corrección de coordenadas, para que la rotación se "pegue" al marcador físico (útil si se quiere una etiqueta que rote con el cartel).
- **Componer rotaciones con Rodrigues** una sola vez, para evitar la dependencia del orden Tait-Bryan (X→Y→Z) cuando los tres offsets son grandes simultáneamente.
- **Aplicar el mismo fix de `gs_ortho`** en la línea 1484 (modo 3D del modelo) si se observan recortes con modelos grandes.

---

**Autor del fix:** sesión de análisis y aplicación 2026-04-25.
**Archivos modificados:** `src/main_filter.c`.
**Archivos consultados para diagnóstico:** `.deps/obs-studio-31.0.0/libobs/graphics/graphics.c` (confirmación del stub), `src/aruco_detector.cpp`, `src/aruco_detector.h`, `src/SJ_3DModel.c` (referencia del pipeline 3D que sí funciona), todos los `.md` de `docs/`.
