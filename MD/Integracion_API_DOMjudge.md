# Integración con API DOMjudge

## Introducción

La integración del plugin de realidad aumentada con sistemas de gestión de concursos de programación requiere mecanismos sofisticados de comunicación de red que permitan obtención en tiempo real de datos dinámicos concernientes al estado de competencias en curso, información de equipos participantes, puntuaciones, envíos de soluciones, y metadatos de problemática. DOMjudge constituye una plataforma ampliamente utilizada en contextos académicos y profesionales para administración de concursos de programación, proporcionando una API RESTful bien documentada que expone datos de torneos, equipos, problemas, y resultados. El presente apartado se dedica al análisis exhaustivo de los mecanismos de integración entre el plugin y la infraestructura de DOMjudge, examinando cómo la arquitectura de cliente HTTP asincrónico (implementada mediante libcurl) facilita comunicación no bloqueante con el servidor remoto, cómo los endpoints específicos de DOMjudge API son interrogados para obtener información heterogénea de torneo, cómo mecanismos de autenticación garantizan acceso controlado a recursos, y cómo datos obtenidos remotamente se transforman mediante deserialización JSON en estructuras de datos internas que habilitan visualización de información dinámica de competencia superpuesta sobre contenido de realidad aumentada. Se analiza particularmente la arquitectura de conexión que aísla complejidad de red, el cliente HTTP asincrónico que permite operación no bloqueante, los endpoints específicos interrogados y su propósito funcional, la autenticación basada en credenciales, el módulo de parseo JSON que transforma datos remotos en representaciones internas, las estructuras de datos que representan información de torneo, equipos, y tabla de posiciones, y finalmente los mecanismos de sincronización entre detección de marcadores ArUco y visualización de información dinámica de equipos.

## 1. Arquitectura de Conexión y Cliente HTTP Asincrónico

### 1.1 Diseño de Capa de Abstracción de Red

La integración con DOMjudge implementa una capa de abstracción de red que aísla la complejidad de comunicación HTTP de la lógica de aplicación del plugin. Esta capa, típicamente denominada módulo web_sync (en archivos web_sync.c y web_sync.h), expone una interfaz simplificada que abstrae los detalles de protocolo HTTP, manejo de sockets, y gestión de conexión.

La razón fundamental de esta abstracción es la naturaleza asincrónica de operaciones de red: una solicitud HTTP a servidor remoto puede tomar décimas de segundo a segundos en completarse, dependiendo de latencia de red, carga del servidor, y tamaño de respuesta. Si las operaciones de red fuesen síncronas (bloqueantes), el hilo principal de OBS se bloquearía durante las solicitudes, causando congelación de retransmisión en vivo. Por lo tanto, la capa de abstracción implementa operaciones de red de forma asincrónica, permitiendo que el hilo principal continúe sin esperar respuestas remotas.

### 1.2 Implementación mediante libcurl y Manejo de Multithreading

libcurl es una biblioteca de cliente HTTP ampliamente utilizada que proporciona interfaces C para solicitudes HTTP. El plugin utiliza libcurl para implementar solicitudes HTTP hacia DOMjudge API. Sin embargo, libcurl en su forma síncrónica bloqueería, por lo que la integración requiere cuidado especial en gestión de threading.

La implementación típica utiliza uno de dos enfoques: (1) multi-threading donde un hilo separado ejecuta solicitudes HTTP sincrónicas mientras el hilo principal continúa renderizado, o (2) utilización de libcurl multi interface que permite gestión no bloqueante de múltiples conexiones simultáneamente. El segundo enfoque es preferible por evitar complejidad de sincronización inter-threads, aunque requiere conocimiento de programación asincrónica basada en event loops.

Internamente, el cliente HTTP mantiene estado de conexión persistente: una conexión TCP se establece una sola vez durante inicialización, siendo reutilizada para múltiples solicitudes posteriores. Esta reutilización de conexión reduce latencia: en lugar de establecer nueva conexión (requiere handshake TCP de 3 pasos, posiblemente seguido de TLS handshake para HTTPS) para cada solicitud, solicitudes posteriores pueden utilizar la conexión existente, reduciendo overhead de latencia.

### 1.3 Gestión de Timeout y Recuperación ante Fallos de Red

Las solicitudes HTTP se configuran con timeouts apropiados: timeout de conexión (típicamente 5-10 segundos) garantiza que si el servidor es completamente inaccesible, la solicitud no se bloquea indefinidamente. Timeout de operación completa (típicamente 30 segundos) garantiza que si el servidor responde lentamente, eventualmente el intento abandona en lugar de causar retraso arbitrario.

Ante fallos de red (conexión rechazada, timeout, DNS falló), el sistema no intenta reintento inmediato, lo que sobrecargaría red e servidor. En su lugar, utiliza estrategia de backoff exponencial: primer reintento después de 5 segundos, segundo después de 10 segundos, tercero después de 20 segundos, con límite máximo (ej. 5 minutos) para evitar esperas indefinidas.

## 2. Endpoints de DOMjudge API y Extracción de Información

### 2.1 GET /api/v4/contests/{contest_id}: Metadatos de Torneo

Este endpoint retorna información general del torneo especificado: nombre, descripción, fechas de inicio y fin, estado actual (no iniciado, en curso, finalizado), y configuración general. Los datos obtenidos incluyen:

- **contest_id**: Identificador único del torneo
- **name**: Nombre completo del torneo (ej. "ACM ICPC Regional 2024")
- **shortname**: Nombre corto para display compacto
- **starttime**: Timestamp UNIX de inicio de torneo
- **endtime**: Timestamp UNIX de fin de torneo
- **state**: Enumeración de estado (scheduled, running, finished)
- **freezetime**: Tiempo en el cual se congela la tabla de posiciones (después no se muestran cambios en vivo)

Esta información permite que el plugin configure su comportamiento apropiadamente: si el torneo no ha iniciado, puede mostrar pantalla de "próximamente"; si está en curso, actualiza información en vivo; si ha finalizado, puede mostrar resultados finales.

### 2.2 GET /api/v4/teams: Información de Equipos Participantes

Este endpoint retorna lista completa de equipos participantes en el torneo, incluyendo:

- **team_id**: Identificador único del equipo (típicamente numérico)
- **name**: Nombre del equipo
- **display_name**: Nombre para display (puede diferir del name)
- **affiliation**: Afiliación institucional (universidad, empresa, etc.)
- **category**: Categoría (estudiantil, profesional, etc.)
- **group**: Grupo o división dentro del torneo
- **country**: País de origen del equipo

El plugin obtiene esta lista una sola vez al inicio o ante cambios de configuración, utilizando la información para construir mapeos entre marcadores ArUco detectados (sección 5.2) y equipos específicos. La lista completa de equipos es típicamente ordenada por identificador, pero puede ser reordenada localmente según ranking actual (obtenido del endpoint scoreboard).

### 2.3 GET /api/v4/scoreboard: Tabla de Posiciones Dinámica

Este es el endpoint más crítico, proporcionando tabla de posiciones actualizada:

- **teams**: Array de objetos, cada uno conteniendo:
  - **team_id**: Referencia a equipo
  - **rank**: Ranking actual (posición 1, 2, 3, etc.)
  - **points**: Puntuación total (número de problemas resueltos)
  - **total_time**: Tiempo acumulado (penalizaciones incluidas)
  - **problems**: Array de objetos describiendo estado de cada problema:
    - **problem_id**: Identificador del problema
    - **solved**: Booleano indicando si fue resuelto
    - **judgement_type_id**: Código de veredicto (AC=Aceptado, WA=Respuesta Incorrecta, TLE=Límite de Tiempo Excedido, etc.)
    - **submission_count**: Número de intentos realizados
    - **time**: Tiempo desde inicio cuando fue resuelto (si fue resuelto)

El plugin consulta este endpoint en cada intervalo de sincronización, siendo la fuente de verdad para información de puntuaciones mostrada en retransmisión.

### 2.4 GET /api/v4/submissions: Detalle de Envíos Individuales

Este endpoint proporciona acceso granular a historial de envíos: cada envío de solución, su tiempo de llegada, veredicto, y problema asociado. Aunque el scoreboard resume esta información, el endpoint submissions permite visualización más detallada o análisis histórico. El plugin típicamente no interroga este endpoint en cada ciclo de sincronización (sería ineficiente), pero puede interrogarlo bajo demanda cuando el operador requiere información detallada de un equipo específico.

## 3. Configuración de Conexión y Autenticación

### 3.1 Parámetros de Configuración Esenciales

La integración con DOMjudge requiere cuatro parámetros configurables críticos:

**api_base_url**: URL base del servidor DOMjudge (ej. `https://domjudge.example.com`). Esta URL parametriza completamente el punto de acceso, permitiendo flexibilidad para diferentes instalaciones de DOMjudge. La URL debe incluir esquema (https requerido en producción), dominio, y opcionalmente puerto si DOMjudge se ejecuta en puerto no estándar.

**contest_id**: Identificador del torneo siendo monitoreo (ej. `acm2024`). DOMjudge permite múltiples torneos simultáneos; este parámetro especifica cuál es objeto de retransmisión. El contest_id debe ser exacto; valor incorrecto resultará en errores 404 (Not Found) cuando se intente acceso.

**api_username** y **api_password**: Credenciales de autenticación requeridas para acceder a DOMjudge API. El usuario debe tener permisos de lectura sobre datos de torneo. Típicamente se crea cuenta específica con permisos limitados para acceso de API, en lugar de utilizar cuenta administrativa.

**sync_interval_sec**: Intervalo de sincronización en segundos (ej. 5 segundos). Determina frecuencia de actualización de información de scoreboard: cada N segundos, se ejecuta solicitud HTTP al servidor para obtener datos actualizados.

### 3.2 Mecanismo de Autenticación HTTP Basic

DOMjudge API típicamente utiliza autenticación HTTP Basic, donde credenciales se codifican en base64 en header Authorization de cada solicitud HTTP:

```
Authorization: Basic base64(username:password)
```

libcurl maneja esta codificación automáticamente si se proporcionan credenciales mediante parámetros CURLOPT_USERPWD. Sin embargo, credenciales en base64 no están cifradas, por lo que HTTPS (en lugar de HTTP) es obligatorio en producción para evitar que credenciales sean interceptadas en tránsito.

Alternativamente, DOMjudge puede estar configurado para soportar tokens de autenticación, donde credenciales username:password se intercambian por token de larga duración que se utiliza en solicitudes posteriores. Esta aproximación es preferible cuando es soportada, ya que reduce exposición de credenciales permanentes.

### 3.3 Validación de Conectividad y Manejo de Errores de Autenticación

Durante inicialización, el sistema realiza "test de conectividad": intenta realizar solicitud simple (ej. GET /api/v4/contests/{contest_id}) para verificar que URL base, contest_id, y credenciales son válidas. Si la solicitud retorna 401 (Unauthorized), se reporta error de credenciales; si retorna 404, se reporta error de contest_id inválido; si retorna error de conexión, se reporta URL base inaccesible.

Esta validación temprana permite detección rápida de problemas de configuración sin esperar a que sincronización falle durante retransmisión en vivo.

## 4. Parseo JSON y Transformación de Datos Remotos

### 4.1 Módulo json_utils y Deserialización de Respuestas

El módulo json_utils (en archivos json_utils.c y json_utils.h) implementa funcionalidad de parseo JSON especializada para transformar respuestas JSON de DOMjudge API en estructuras de datos C internas. El módulo actúa como capa de transformación: recibe strings JSON brutos, los valida y parsea, y produce estructuras C fuertemente tipadas.

La implementación típicamente utiliza biblioteca de parseo JSON estándar (como CJSON, Jansson, o similar), que proporciona APIs para navegar estructuras JSON. json_utils construye una capa de nivel superior que conoce la estructura específica esperada de respuestas de DOMjudge, proporcionando funciones como `parse_scoreboard_json()`, `parse_teams_json()`, etc.

### 4.2 Conversión de Formato y Validación de Integridad

Cada función de parseo implementa validación de estructura: verifica que campos esperados están presentes, tienen tipos de datos correctos (números son números, strings son strings, arrays son arrays), y valores están en rangos válidos.

Por ejemplo, al parsear scoreboard:
- Se verifica que existe campo "teams" y es array
- Para cada elemento del array, se verifica presencia de "team_id" (debe ser número), "rank" (número), "points" (número), "total_time" (número o null), "problems" (array)
- Para cada problema, se verifica "problem_id" (número), "solved" (booleano), "judgement_type_id" (string o null)

Campos opcionales (que pueden estar presentes o ausentes) se marcan como opcionales durante parseo, permitiendo que deserialización suceda incluso si el campo falta.

### 4.3 Manejo de Valores Nulos y Campos Opcionales

JSON permite valores nulos, que pueden tener significado semántico diferente a ausencia de campo. Por ejemplo, "total_time": null puede significar que un equipo no ha resuelto ningún problema, mientras que que el campo total_time esté completamente ausente puede indicar que el servidor no envió esa información.

El módulo json_utils debe manejar estas distinciones cuidadosamente: utilizando representaciones internas que permitan distinguir "valor nulo" de "ausente". En C, esto típicamente se implementa mediante campos adicionales de "presencia" (booleanos que indican si campo fue proporcionado).

### 4.4 Codificación UTF-8 y Compatibilidad de Caracteres

DOMjudge puede retornar información con caracteres internacionales (nombres de equipos en cirílico, caracteres acentuados, ideogramas, etc.), todos codificados en UTF-8. El parseo JSON debe preservar UTF-8 correctamente durante transformación de string JSON a estructuras internas.

Cuando se renderiza esta información (nombres de equipos, etc.), el sistema debe garantizar que fuentes de renderizado soportan caracteres presentes. Si se utiliza fuente que solo soporta ASCII, caracteres no ASCII se renderizarán como caracteres de reemplazo, degradando legibilidad.

## 5. Estructuras de Datos de Torneo y Representación Interna

### 5.1 Metadatos de Torneo y Configuración

La estructura interna que representa metadatos del torneo típicamente contiene:

```c
typedef struct {
    char contest_id[256];
    char name[512];
    char shortname[128];
    time_t starttime;
    time_t endtime;
    enum { SCHEDULED, RUNNING, FINISHED } state;
    time_t freezetime;
    int num_problems;
    int num_teams;
} contest_info_t;
```

Esta estructura se completa una sola vez al inicio durante sincronización inicial, permitiendo que otras partes del plugin accedan a metadatos del torneo sin necesidad de consulta remota.

### 5.2 Información de Equipos y Tabla de Posiciones Dinámica

La información de equipos se mantiene en estructura:

```c
typedef struct {
    int team_id;
    char name[256];
    char display_name[256];
    char affiliation[512];
    char category[64];
    int rank;
    int points;
    int total_time;
    problem_status_t *problems; // Array dinámico
    int num_problems;
} team_info_t;

typedef struct {
    team_info_t *teams;
    int num_teams;
    time_t last_update;
} scoreboard_t;
```

El módulo aruco_detector (sección 5.2) mantiene mapeos entre marcadores ArUco detectados (identificados por ID de marcador) y equipos (identificados por team_id). Cuando se detecta un marcador específico, se usa su mapeo para acceder información del equipo asociado, permitiendo renderizado de información dinámica específica a ese equipo.

### 5.3 Datos de Problemas y Veredictos

Cada problema tiene representación que captura su estado resuelto/no resuelto:

```c
typedef struct {
    int problem_id;
    char problem_code[16];
    char problem_name[256];
    bool solved;
    char judgement_type[32]; // "AC", "WA", "TLE", "RE", etc.
    int submission_count;
    int time_minutes;
} problem_status_t;
```

Esta información permite renderizado detallado del progreso de equipo: qué problemas ha resuelto, cuáles ha intentado sin éxito, cuántos intentos ha realizado.

## 6. Sincronización entre Detección AR y Visualización de Información Dinámica

### 6.1 Mapeo de Marcadores a Equipos

La característica fundamental que integra detección de realidad aumentada con información de competencia es el mapeo entre marcadores ArUco y equipos DOMjudge. Este mapeo permite que cuando un marcador específico es detectado, el sistema identifique qué equipo está siendo representado, permitiendo renderizado de información dinámica específica a ese equipo.

El mapeo se mantiene como tabla con entrada máxima de 16 asociaciones (constante MAX_TEAM_INF):

```c
typedef struct {
    int marker_id; // ID del marcador ArUco detectado
    int team_id;   // ID del equipo DOMjudge mapeado
} marker_team_mapping_t;

typedef struct {
    marker_team_mapping_t mappings[MAX_TEAM_INF];
    int num_mappings;
} marker_team_mappings_t;
```

Este mapeo es configurable por operador: a través de interfaz de propiedades de OBS, se puede especificar que marcador ID 5 corresponde a equipo 123, marcador 10 corresponde a equipo 456, etc.

### 6.2 Modo de Visualización Dinámico y Fallback

El sistema implementa dos modos de visualización que interconexionan detección y información de torneo:

**Modo Scoreboard Completo**: Cuando no se detectan marcadores ArUco, o cuando operador ha seleccionado explícitamente modo de visualización de torneo, se renderiza tabla completa de posiciones, mostrando todos los equipos ordenados por ranking, con puntuaciones y tiempos. Este modo proporciona contexto general de competencia.

**Modo Equipo Detectado**: Cuando se detecta un marcador ArUco específico que tiene mapeo válido a equipo, se cambia visualización para mostrar información destacada de ese equipo en particular: nombre prominente, puntuación actual, historial de resolución de problemas, información de intentos fallidos. Este modo proporciona información contextual específica al modelo 3D superpuesto.

La transición entre modos es suave: cuando un marcador entra a vista (comienza a detectarse), la visualización se anima desde modo scoreboard hacia modo equipo; cuando sale de vista, se anima de regreso a scoreboard. Esto evita cambios abruptos que serían desconcertantes visualmente.

### 6.3 Actualización Temporal y Sincronización de Información Dinámica

Durante cada ciclo de sincronización remota (cada sync_interval_sec segundos), se obtienen nuevos datos de scoreboard del servidor DOMjudge. Si se encuentra que información de equipo actualmente detectado ha cambiado (nueva resolución de problema, cambio de ranking, etc.), la visualización en pantalla se actualiza en tiempo real, reflejando cambios del servidor.

Los cambios se visualizan con animaciones suaves: si un equipo resolvió nuevo problema, se anima el contador de problemas resueltos incrementando; si cambió ranking, se anima la posición del equipo en la tabla. Estas animaciones proporcionan retroalimentación visual clara de que información dinámica está siendo actualizada.

### 6.4 Renderizado de Información de Equipo Detectado

Cuando se detecta un marcador con mapeo válido, se renderiza información del equipo asociado como overlay en proximidad del modelo 3D superpuesto:

- **Nombre del equipo**: Renderizado prominentemente, típicamente arriba o abajo del modelo 3D
- **Puntuación actual**: Número de problemas resueltos, con cambios incrementales animados
- **Tiempo total**: Tiempo acumulado con penalizaciones, permitiendo desempate visual
- **Historial de problemas**: Array visual mostrando estado (AC vs intentos fallidos) de cada problema
- **Indicador de actividad**: Si el equipo realizó envíos recientemente, indicador visual lo señala

El renderizado respeta el offset visual configurado del usuario (sección 6.5 en documento anterior), permitiendo posicionamiento flexible adaptado a composición visual deseada.

### 6.5 Display de Scoreboard Completo con Integración Visual

El scoreboard completo se renderiza como tabla estructurada:

```
Pos  Nombre del Equipo        Pts  Tiempo  A  B  C  D  E
 1   Universidad XYZ            5  247:30  ✓  ✓  ✓  ✓  ○
 2   Instituto ABC              5  251:15  ✓  ✓  ✓  ✓  ○
 3   Empresa TechCorp           4  189:45  ✓  ✓  ✓  ○  ✓
 ...
```

Donde: Pos=Ranking, Pts=Problemas resueltos, Tiempo=Tiempo acumulado, A-E=Estado individual de cada problema (✓=AC/resuelto, ○=no intentado, X=intentado pero no resuelto).

El equipo actualmente detectado (si existe mapeo válido) es resaltado visualmente (ej. fondo coloreado) para indicar que la información de ese equipo es expandida en overlay separado.

---

## Conclusión

La integración con API DOMjudge implementada constituye infraestructura sofisticada que entra dos subsistemas heterogéneos: infraestructura local de retransmisión de realidad aumentada (OBS Studio con plugin personalizado) e infraestructura remota de gestión de concursos (DOMjudge). La arquitectura de capa de abstracción de red, implementada mediante cliente HTTP asincrónico basado en libcurl, garantiza que operaciones de red no bloquean la retransmisión en vivo. La interrogación de múltiples endpoints de DOMjudge API proporciona acceso a metadatos de torneo, información de equipos, tabla de posiciones dinámica, e historial detallado de envíos. El módulo json_utils implementa deserialización confiable de respuestas JSON heterogéneas, transformando datos remotos en estructuras C internas fuertemente tipadas. Las estructuras de datos internas representan información de torneo, equipos, problemas, y veredictos, permitiendo acceso eficiente durante renderizado. Finalmente, el mapeo configurable entre marcadores ArUco y equipos DOMjudge habilita la característica central del sistema: sincronización entre detección de realidad aumentada y visualización de información dinámica de competencia, permitiendo que retransmisiones de concursos de programación muestren información en vivo de torneos integrada visualmente con modelos 3D superpuestos, enriqueciendo significativamente la experiencia visual de audiencias remotas.
