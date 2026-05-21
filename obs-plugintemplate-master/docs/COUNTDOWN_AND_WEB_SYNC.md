# Reloj de cuenta atrás y sincronización web

## Resumen

Se ha integrado en el plugin SJ_3D un **modo Countdown** que muestra un reloj analógico 3D cuyas manecillas indican el **tiempo restante** (countdown). La sincronización con una API REST externa es **opcional** y se realiza en un hilo secundario; las peticiones HTTP **nunca bloquean el hilo de render** de OBS.

## Modificaciones realizadas (incremental)

### 1. Módulo `countdown_clock` (lógica interna del reloj)

- **Archivos:** `src/countdown_clock.h`, `src/countdown_clock.c`
- **Responsabilidad:** Estado del countdown (duración, tiempo restante, ejecución/pausa), cálculo de ángulos de las manecillas (hora, minuto, segundo) para un reloj que muestra tiempo restante.
- **API principal:**
  - `countdown_clock_create` / `countdown_clock_destroy`
  - `countdown_clock_set_duration_hms`, `countdown_clock_start`, `countdown_clock_pause`, `countdown_clock_resume`, `countdown_clock_reset`
  - `countdown_clock_tick(clock, delta_seconds)` — llamar desde `video_tick` con el delta de tiempo.
  - `countdown_clock_get_hand_angles` — devuelve ángulos en grados (0–360) para las tres manecillas.
  - `countdown_clock_sync_remaining` — permite corregir el tiempo restante desde una fuente externa (p. ej. API web).

### 2. Módulo `web_sync` (sincronización web)

- **Archivos:** `src/web_sync.h`, `src/web_sync.c`
- **Responsabilidad:** Consultar periódicamente una URL que devuelve JSON con `hours`, `minutes`, `seconds` (tiempo restante). Las peticiones se ejecutan en un **hilo dedicado**; el hilo de render solo lee el resultado en `web_sync_poll()` (sin bloqueo prolongado).
- **Formato esperado de la API:**  
  `{ "hours": 1, "minutes": 30, "seconds": 0 }`
- **Dependencia:** libcurl (y pthread para el hilo).
- **API principal:**
  - `web_sync_create(url, interval_seconds)`, `web_sync_destroy`
  - `web_sync_set_url`, `web_sync_set_interval`, `web_sync_set_enabled`
  - `web_sync_poll(sync, &result)` — llamar desde el hilo principal (p. ej. `filter_tick`); devuelve `true` si hay datos nuevos y los copia en `result`.

### 3. Extensión del render 3D para modo reloj

- **Archivos:** `src/SJ_3DModel.h`, `src/SJ_3DModel.c`
- **Cambio:** Nueva función `render_model_clock_c(...)` que aplica rotaciones extra por malla para las manecillas:
  - **Convención:** mesh 0 = esfera/dial (sin rotación extra), mesh 1 = manecilla de horas, mesh 2 = minutos, mesh 3 = segundos.
  - Cada manecilla recibe una rotación adicional alrededor del eje Z (grados) según el tiempo restante.

### 4. Integración en el filtro (main_filter.c)

- **Modo de renderizado:** Se añade la opción **"Countdown (reloj)"** (valor 2) junto a "3D" y "AR".
- **Propiedades de Countdown:**
  - Duración: horas, minutos, segundos.
  - "Countdown en marcha" (checkbox): inicia o pausa.
  - "Reiniciar countdown" (marcar y desmarcar): reinicia al valor de duración configurado.
- **Sincronización web (opcional):**
  - "Sincronización web activa", "URL API", "Intervalo sincronización (seg)".
  - Si está activa y hay URL, se crea/actualiza `web_sync`; en `filter_tick` se llama `web_sync_poll` y, si hay resultado válido, `countdown_clock_sync_remaining`.
- **Render:** En modo Countdown se usa `render_model_clock_c` con los ángulos devueltos por `countdown_clock_get_hand_angles`. El modelo 3D se carga y muestra con la misma lógica que en modo 3D (ruta del modelo, textura, posición, escala, rotación base).

### 5. CMake

- **Nuevos fuentes:** `src/countdown_clock.c`, `src/web_sync.c`.
- **Dependencias:** `find_package(CURL REQUIRED)`, `find_package(Threads REQUIRED)`; `target_link_libraries(..., CURL::libcurl, Threads::Threads)`.
- **Nota:** En Windows se enlaza **explícitamente** w32-pthreads desde **Dependencies/w32-pthreads** (includes y `pthreadVC3.lib` / `pthreadVC3d.lib`). No se usa `Threads::Threads`.

## Uso del modelo 3D para el reloj

- El modelo debe tener al menos una malla (dial). Para que las manecillas se muevan:
  - **Mesh 0:** Cuerpo/dial del reloj (sin rotación extra).
  - **Mesh 1:** Manecilla de horas (rotación Z = tiempo restante en horas, escalado al dial, p. ej. 12 h).
  - **Mesh 2:** Manecilla de minutos (rotación Z = minutos restantes).
  - **Mesh 3:** Manecilla de segundos (rotación Z = segundos restantes).
- Si el modelo tiene menos de 4 mallas, solo se aplica rotación extra a las que existan (1, 2 o 3).
- La carga del modelo y la textura se hace igual que en modo 3D (ruta del modelo, ruta de textura en las propiedades del filtro).

## API REST de ejemplo

La URL configurada debe responder a GET con JSON como:

```json
{
  "hours": 1,
  "minutes": 30,
  "seconds": 45
}
```

Se usa como **corrección/sincronización** del countdown local: cuando llega una respuesta válida, el tiempo restante del reloj se ajusta a esos valores. El countdown local sigue restando tiempo entre peticiones; la API solo corrige la deriva.

## Extensibilidad

- **countdown_clock:** Se puede ampliar con más estados, dial de 24 h (`countdown_clock_set_dial_hours`), o suavizado de ángulos.
- **web_sync:** Se puede cambiar el formato JSON (p. ej. tiempo absoluto en lugar de restante) ampliando el parser en `web_sync.c`.
- **render_model_clock_c:** La convención de índices de malla (0=dial, 1=h, 2=m, 3=s) se puede hacer configurable (p. ej. por nombres de malla) en el futuro.
