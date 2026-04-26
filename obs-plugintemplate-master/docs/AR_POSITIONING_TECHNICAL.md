# Documentación Técnica: Posicionamiento AR y Composición de Overlays

Este documento detalla la arquitectura técnica, las matemáticas y el flujo de renderizado del sistema de Realidad Aumentada (AR) implementado en el plugin.

---

## 1. Arquitectura de Coordenadas

La base de un seguimiento AR preciso es la sincronización de los sistemas de coordenadas entre la cámara (OpenCV) y el motor gráfico (OBS/OpenGL).

### El Sistema Top-Down (Y-Down)
Hemos estandarizado el plugin para trabajar en **Y-Down**, donde:
- **(0,0)** es la esquina superior izquierda.
- **X+** aumenta hacia la derecha.
- **Y+** aumenta hacia **abajo**.

#### Correspondencia con OpenCV
OpenCV entrega los corners de los marcadores en este formato exacto. Al configurar OBS con `gs_ortho(0, W, 0, H)` (que en su contexto de filtros nativamente sitúa el 0 arriba), logramos una **correspondencia 1:1**. 

> [!NOTE]
> Esta sincronización elimina la necesidad de transformaciones manuales `Altura - Y`, reduciendo la carga computacional y evitando errores de inversión del eje Y al mover el marcador físicamente.

---

## 2. Funciones Principales y su Lógica

### `process_frame_rgba` (aruco_detector.cpp)
Es la puerta de entrada para la detección. Su función es:
1. Detectar marcadores en el frame de vídeo bruto capturado por el plugin.
2. **Escalado de Píxeles:** Convierte las coordenadas del frame interno de la cámara (ej. 1280x720) a la resolución real de la fuente en OBS (ej. 1920x1080).
   $$scale\_x = \frac{W_{source}}{W_{frame}}$$
3. Almacenar los 4 "corners" escalados en la estructura `ArucoResult` para su uso en el hilo de renderizado.

### `aruco_marker_metrics_2d` (Cubo-plugin.c)
Analiza la geometría de los corners para derivar métricas útiles para el renderizado:
- **Centro Geométrico ($C_x, C_y$):** Se calcula como el promedio simple para máxima estabilidad horizontal.
  $$C_x = \frac{\sum x_i}{4}, \quad C_y = \frac{\sum y_i}{4}$$
- **Dimensiones Proyectadas:** Calcula el ancho y alto visual (`marker_w`, `marker_h`) basándose en la distancia euclidiana entre esquinas adyacentes.

### `filter_render` (Cubo-plugin.c)
El núcleo del renderizado visual. Utiliza el **Stack de Matrices** de OpenGL para posicionar el texto con precisión clínica:
1. `gs_matrix_push()`: Aísla las transformaciones del overlay del resto de la escena.
2. `gs_matrix_translate3f(C_x, C_y, 0)`: Posiciona el "pincel" exactamente en el centro del marcador ArUco.
3. `gs_matrix_scale3f(S_x, S_y, 1)`: Aplica el factor de escala calculado para que el texto encaje en el espacio del marcador.
4. `gs_matrix_translate3f(-tw/2, -th/2, 0)`: Desplaza el contenido media altura y medio ancho hacia atrás para que su centro coincida con el origen de la matriz (el centro del QR).

---

## 3. Composición Multi-Fuente (Scoreboard Profesional)

En el **Modo 3 (Scoreboard)**, el plugin combina 4 fuentes de texto individuales (`sb_pos_source`, `sb_name_source`, `sb_solved_source`, `sb_time_source`) para formar un único overlay cohesionado.

### ¿Cómo se combinan en un solo bloque?
Aunque son fuentes independientes gestionadas por OBS, se renderizan secuencialmente dentro de la misma transformación matricial AR.

#### Paso 1: Suma de Masas (Aggregate Width)
Calculamos el ancho total ($T_w$) de todo el conjunto sumando los anchos de cada columna más los espacios (`col_gap`):
$$T_w = W_{pos} + gap + W_{name} + gap + W_{res} + gap + W_{time}$$

#### Paso 2: El Cursor de Renderizado
Usamos una variable acumuladora `x_cursor`. El proceso es:
- Columna 1 en `x_cursor` (0.0). Se dibuja.
- Incrementamos `x_cursor += W_col1 + gap`.
- Columna 2 en `x_cursor`. Se dibuja.
- (Repetir para todas las columnas).

Debido a que todas se dibujan tras la orden `gs_matrix_translate3f(C_x, C_y, 0)`, todas heredan el mismo movimiento AR, manteniendo la alineación entre columnas de forma perfecta aunque el marcador se mueva rápido.

---

## 4. Estabilización (Smoothing)

Para filtrar el ruido de la cámara que causa que el texto "tiemble", aplicamos un filtro **LERP (Linear Interpolation)**:

$$P_{smooth} = (1 - \alpha) \cdot P_{smooth\_old} + \alpha \cdot P_{new}$$

Esto garantiza que el posicionamiento sea fluido y "clínico", permitiendo que el overlay parezca pegado con adhesivo físico al marcador QR en el mundo real.
