# Guía de Sincronización Web con DOMjudge 🏆

Esta guía explica detalladamente cómo configurar y utilizar el sistema de sincronización web del plugin para conectar con servidores **DOMjudge** y obtener datos de concursos en tiempo real.

## 🚀 Requisitos Críticos (DLLs)

Para que el soporte HTTPS (SSL) funcione, el plugin depende de una versión específica de **libcurl**.

### Ubicación del archivo necesario
El archivo `libcurl.dll` con soporte SSL se encuentra en:
`[Carpeta del Proyecto]\.deps\obs-deps-2024-09-12-x64\bin\libcurl.dll`

### Instalación
1.  **Copia** `libcurl.dll` desde la ruta anterior.
2.  **Pégalo** en la carpeta de ejecución de OBS: `C:\Program Files\obs-studio\bin\64bit\`.
3.  **Sobrescribe** si existe uno previo (esto restaura el soporte SSL en todo OBS).

> [!IMPORTANT]
> Si el archivo `libcurl.dll` no tiene soporte SSL (Schannel/OpenSSL), OBS mostrará un error de `Protocol "https" not supported` y no se podrá sincronizar nada.

---

## ⚙️ Configuración en OBS

Una vez cargado el filtro `SJ_3D` o configurado el modo `Scoreboard`, aparecerán los siguientes campos:

| Campo | Descripción | Ejemplo (Demo) |
| :--- | :--- | :--- |
| **Sincronizar con DOMjudge** | Activa/Desactiva las peticiones automáticas. | Activado |
| **URL API (DOMjudge)** | La URL base de la API REST v4. **No** es la URL pública de la web. | `https://www.domjudge.org/demoweb/api/v4` |
| **Contest ID** | El identificador único del concurso. | `nwerc18` |
| **Usuario API** | Nombre de usuario para concursos privados (opcional). | *(vacío para la demo)* |
| **Contraseña API** | Contraseña del usuario (opcional). | *(vacío para la demo)* |
| **Intervalo Sinc (seg)** | Frecuencia con la que el plugin consulta los datos. | `10` segundos |

---

## 🔍 Prueba de Conexión

El botón **"Probar Conexión"** realiza una petición instantánea para validar los datos introducidos.

1.  Construye la URL: `{URL API}/contests/{Contest ID}`.
2.  Intenta obtener los detalles del concurso (nombre, fecha de inicio, etc.).
3.  **ÉXITO**: Si el servidor responde con un JSON válido del concurso, verás en el log:
    `[WEB_SYNC] Conexión OK: contest id='nwerc18' (HTTP 200)`
4.  **FALLO**: Si hay un error 404, revisa que la URL no termine en `/public`. Si hay un error 401, revisa el usuario/contraseña.

---

## 🛠️ Funcionamiento Técnico

El sistema funciona mediante tres hilos/etapas:

1.  **Hilo de Sincronización (Second Thread)**: Realiza peticiones `GET` asíncronas cada X segundos usando `libcurl`. Nunca bloquea la interfaz de OBS.
2.  **Caché de Equipos**: Al recibir el "Scoreboard", el plugin descarga la lista de equipos (`/teams`) para obtener los nombres reales, ya que el scoreboard solo envía IDs por eficiencia.
3.  **Actualización de UI (filter_tick)**: Cada frame de OBS, el plugin comprueba si hay nuevos datos descargados. Si los hay:
    *   Actualiza el reloj (si está en modo cuenta atrás).
    *   Genera el texto formateado del Scoreboard.
    *   Actualiza la fuente de texto interna que se muestra en pantalla.

---

## 📝 Ejemplo de Configuración para Servidor de Pruebas

Para probarlo ahora mismo con la demo oficial de DOMjudge:
- **URL**: `https://www.domjudge.org/demoweb/api/v4`
- **Contest**: `nwerc18`
- **Credenciales**: Dejar vacías (es acceso público).

Si los nombres de los equipos no aparecen inmediatamente, espera unos segundos a que el plugin termine de sincronizar la lista completa de participantes en segundo plano.
