# Liberación de Recursos y Limpieza

## Introducción

La gestión completa del ciclo de vida de recursos en aplicaciones de software de producción requiere mecanismos robustos de inicialización y, correspondientemente, liberación ordenada de recursos al momento de terminación de aplicación o desactivación de componentes. En contextos de plugins para OBS Studio, donde múltiples plugins pueden ser cargados y descargados dinámicamente, la liberación inadecuada de recursos ocasiona fugas de memoria que degradan el rendimiento del sistema anfitrión, consumo excesivo de memoria GPU, y potencialmente bloqueos de archivos que impiden operaciones posteriores. El presente apartado se dedica al análisis exhaustivo de estrategias y mecanismos de limpieza implementados en el plugin, examinando cómo cada categoría de recurso (geometría 3D en memoria GPU, texturas, estructuras de datos de detección ArUco, conexiones de red, y datos dinámicos de cuenta atrás) se libera de forma segura y ordenada, cómo se evita liberación de recursos aún en uso, cómo se garantiza que operaciones de limpieza se completan incluso ante errores parciales, y cómo el sistema mantiene estado consistente durante y después de operaciones de limpieza. Se analiza particularmente la destrucción de mallas y buffers de GPU que requiere coordinación con contexto gráfico, la liberación de texturas que debe ser sincronizada con detenciones de renderizado, el cleanup de módulos especializados como detector ArUco y cliente web, y finalmente las estrategias de validación que garantizan que recursos se liberan completamente sin dejar referencias pendientes.

## 1. Destrucción de Mallas y Liberación de Buffers de GPU

### 1.1 Ciclo de Vida de Recursos Gráficos en Memoria GPU

Los recursos gráficos (mallas, buffers de vértices, buffers de índices) residen en memoria de GPU, un espacio de memoria separado del heap de CPU. La creación de estos recursos requiere operaciones de API gráfica específicas (OpenGL glGenBuffers/glBufferData, DirectX ID3D11Device::CreateBuffer, Metal MTLBuffer creation), y correspondientemente, la liberación requiere operaciones de API gráfica simétricas (glDeleteBuffers, ID3D11Resource::Release, [MTLBuffer release]).

La característica crítica es que estos recursos deben ser liberados únicamente desde el contexto gráfico correcto: el hilo que posee el contexto gráfico actual. Si se intenta liberar un recurso de GPU desde hilo diferente, o después de que el contexto gráfico ha sido destruido, la liberación falla, resultando en corrupción de estado gráfico o crashes de aplicación.

### 1.2 Destrucción Ordenada de Mallas y Estructura Jerárquica

Un modelo 3D típicamente consiste de múltiples mallas subordinadas, cada una con buffers de vértices, buffers de índices, y referencias a materiales. La destrucción de un modelo requiere recursivamente destruir todas sus mallas subordinadas en orden correcto: primero se destruyen buffers de índices, luego buffers de vértices, luego se liberan referencias a materiales (que pueden ser compartidos con otras mallas), y finalmente se libera la estructura de datos de malla misma.

El orden es importante: si se intenta acceder a un buffer ya destruido (por ejemplo, durante renderizado asincrónico que aún no ha completado), se produce error de acceso a memoria inválida. La implementación típicamente utiliza modelo de conteo de referencias (reference counting) donde cada buffer de GPU mantiene contador de cuántas mallas lo referencian; el buffer solo se destruye cuando el contador llega a cero.

### 1.3 Sincronización con Pipeline de Renderizado

La destrucción de buffers de GPU debe sincronizarse con el pipeline de renderizado: si un buffer está siendo utilizado en renderizado en curso (múltiples fotogramas pueden estar en diferentes etapas de procesamiento simultáneamente en GPUs modernas), su destrucción inmediata causaría que comandos de renderizado pendientes intenten acceder a memoria inválida.

La estrategia de sincronización típicamente implica: (1) marcar el buffer como "pendiente de destrucción" sin liberarlo inmediatamente, (2) permitir que todos los comandos de renderizado pendientes completen, (3) recién entonces liberar el buffer de GPU. Alternativamente, se pueden utilizar "disposal frames" donde buffers marcados para destrucción se liberan solo después de N fotogramas, garantizando que cualquier renderizado pendiente habrá completado.

### 1.4 Validación y Detección de Fugas de Memoria de GPU

Para garantizar que todos los buffers de GPU se liberan correctamente, el sistema mantiene registro de todos los buffers creados (típicamente en una estructura de lista dinámica o tabla hash), y durante limpieza final, verifica que la lista está vacía. Si quedan buffers sin liberar, se reporta advertencia de fuga de memoria, permitiendo diagnóstico de componentes que no liberan recursos apropiadamente.

Algunas plataformas y herramientas de desarrollo (como NVIDIA NSight, AMD RenderDoc) proporcionan herramientas de profiling que pueden detectar fugas de memoria de GPU directamente, identificando buffers que fueron creados pero nunca liberados. Estas herramientas son valiosas durante desarrollo y prueba.

## 2. Liberación de Texturas y Gestión de Recursos de Imagen

### 2.1 Ciclo de Vida de Texturas GPU y Almacenamiento Dual

Una textura típicamente existe en dos formas: (1) datos de imagen en memoria CPU (cargados desde archivo de disco), y (2) objeto de textura GPU (cargado en memoria GPU para renderizado rápido). Durante carga de textura, ambas formas se crean; durante liberación, ambas deben destruirse.

La liberación de imagen CPU es simple: se libera memoria del buffer que contiene píxeles. La liberación de textura GPU es más compleja: requiere operación de API gráfica y sincronización con renderizado en curso, similar a buffers de vértices. Adicionalmente, texturas pueden tener metadatos asociados: mipmaps precalculados (versiones de baja resolución para renderizado optimizado a distancia), que también deben liberarse.

### 2.2 Estrategia de Liberación en Cascada

Un modelo 3D puede referenciar múltiples texturas (diferentes texturas para diferentes mallas, o múltiples texturas por malla para casos complejos). La destrucción de un modelo debe liberar todas las texturas que referenciona, pero hacerlo cuidadosamente: si múltiples modelos referencian la misma textura, la textura no debe liberarse hasta que todos los modelos que la referencian han sido destruidos.

La estrategia de conteo de referencias se aplica igualmente a texturas: cada textura mantiene contador de cuántos modelos/mallas la referencian; se libera únicamente cuando el contador llega a cero.

### 2.3 Limpieza de Caché de Texturas

Para evitar recargar la misma textura múltiples veces desde disco, el sistema mantiene caché de texturas cargadas. Este caché mapea rutas de archivo a objetos de textura GPU. Cuando se libera una textura de GPU, se debe también remover de caché, permitiendo que si la textura se carga nuevamente después, se realiza recarga fresca desde disco.

La limpieza del caché requiere cuidado: si la textura está siendo renderizada actualmente (referenciada por caché), no puede ser destruida. La estrategia típica es marcar entradas de caché como "evicted" cuando se solicita liberación, permitiendo que renderizado en curso complete antes de destruir.

### 2.4 Validación de Liberación Completa de Recursos de Imagen

Análogamente a buffers de GPU, el sistema mantiene registro de todas las texturas creadas. Durante limpieza, se verifica que todas las texturas han sido liberadas (caché de texturas está vacío, no hay referencias pendientes). Si texturas permanecen sin liberar, se reporta diagnóstico.

## 3. Cleanup del Detector ArUco y Estructuras de Visión por Computadora

### 3.1 Destrucción del Detector y Liberación de Parámetros

El módulo aruco_detector (sección 4 del apartado anterior) mantiene estado interno significativo: instancia de diccionario ArUco, parámetros de calibración de cámara cargados en memoria, buffers temporales utilizados durante detección. La función cleanup debe destruir todos estos componentes ordenadamente.

El diccionario ArUco es típicamente objeto gestionado por OpenCV que se destruye simplemente mediante liberación de referencia. Los parámetros de calibración se almacenan como matrices numéricas (matriz de cámara 3×3, vector de coeficientes de distorsión) que se liberan como estructuras de datos C estándar. Los buffers temporales son asignaciones de memoria estándar que se liberan mediante free() o delete[].

### 3.2 Limpieza de Estructuras de Seguimiento Temporal

Si se implementaron filtros de Kalman u otros mecanismos de seguimiento temporal (para suavizar transiciones entre poses estimadas fotograma-a-fotograma), estos también deben limpiarse. Los filtros mantienen estado interno (última estimación conocida, matriz de covarianza, etc.) que debe reinicializarse o liberarse.

### 3.3 Cierre Ordenado de Recursos del Detector

El detector ArUco puede mantener conexiones a otros módulos (carga de calibración desde archivo requiere manejo de archivos, etc.). Durante cleanup, se debe garantizar que estos accesos se cierran: archivos abiertos se cierran, buffers de lectura se liberan, etc.

### 3.4 Registro de Estado de Limpieza

El módulo aruco_detector debe reportar al módulo padre su estado de limpieza: si cleanup completó exitosamente, si encontró condiciones de error (buffers no vaciados, referencias pendientes), etc. Esto permite diagnóstico de problemas de limpieza incompleta.

## 4. Liberación de Estructuras de Datos Dinámicas y Metadatos

### 4.1 Destrucción de Datos de Torneo y Scoreboard

Cuando se utiliza integración con DOMjudge (sección 7), el plugin mantiene estructuras en memoria que representan información de torneo: lista de equipos, datos de scoreboard actualizado, mapeos entre marcadores y equipos. Estas estructuras son dinámicas y asignadas mediante malloc (o new en C++). Durante limpieza, deben liberarse completamente.

La estructura de scoreboard contiene array dinámico de equipos, cada uno con array dinámico de problemas. La liberación debe recursivamente liberar arrays subordinados:

```c
// Pseudocódigo de limpieza
for (int i = 0; i < scoreboard->num_teams; i++) {
    free(scoreboard->teams[i].problems);
}
free(scoreboard->teams);
free(scoreboard);
```

### 4.2 Limpieza de Mapeos de Marcadores a Equipos

Los mapeos entre marcadores ArUco y equipos DOMjudge (tabla de hasta 16 asociaciones) son estructuras simples que pueden liberarse directamente. Sin embargo, si estos mapeos referenciaban memoria dinámicamente asignada (strings de nombres de equipos cacheados, etc.), esa memoria también debe liberarse.

### 4.3 Validación de Integridad de Estructura

Durante limpieza, el sistema puede realizar validaciones para detectar corrupción: verificar que punteros dentro de estructuras no apuntan a memoria ya liberada, que contadores internos tienen valores válidos, etc. Estas validaciones mejoran robustez detectando bugs de corrupción de memoria antes de que causen crashes.

### 4.4 Destrucción de Caché de Datos

Si el sistema mantiene caché de respuestas de API previamente obtenidas (para acelerar operaciones posteriores), esta caché debe vaciarse durante limpieza. Los datos cacheados son típicamente malloc'd y deben liberarse.

## 5. Destrucción del Reloj de Cuenta Atrás y Limpieza de Temporizadores

### 5.1 Ciclo de Vida de Componentes de Temporización

El reloj de cuenta atrás (countdown clock) es componente que mantiene estado temporal: tiempo de inicio, duración total, tiempo final esperado, estado actual (no iniciado, en curso, finalizado). Durante limpieza, este estado debe ser destruido.

Los temporizadores del sistema (timers que disparan callbacks en intervalos regulares) deben cancelarse. Si se había registrado timer para actualizar contador cada segundo, ese timer debe ser deregistrado antes de que el componente sea destruido, evitando que timer continúe intentando llamar callbacks después de destrucción.

### 5.2 Geometría y Texturas Específicas del Reloj

Si el reloj de cuenta atrás utiliza geometría 3D personalizada (por ejemplo, un cubo 3D con números renderizados), esa geometría es malla 3D que debe destruirse siguiendo los procedimientos de destrucción de mallas (sección 1).

Las texturas del reloj (si se utiliza textura precalculada de números) deben liberarse como texturas normales (sección 2).

### 5.3 Parámetros de Configuración Visual

Parámetros de visualización del reloj (fuente, tamaño, color) pueden ser almacenados como cadenas o estructuras dinámicas que deben liberarse: típicamente strings de nombre de fuente, buffers de fuente cacheada, etc.

### 5.4 Limpieza de Referencias Residuales

El módulo de reloj puede mantener referencias a otros componentes del sistema (referencia a detector ArUco para posicionamiento relativo al marcador detectado, referencia a configuración global, etc.). Durante limpieza, estas referencias deben invalidarse, evitando que código residual intente acceder a módulo después de su destrucción.

## 6. Cleanup de Cliente Web y Terminación de Conexiones de Red

### 6.1 Cierre Ordenado de Conexiones HTTP

El cliente web (módulo web_sync que conecta a DOMjudge API) mantiene conexión persistente al servidor remoto. Durante limpieza, esta conexión debe cerrarse ordenadamente: se envía comando de desconexión si el protocolo lo soporta, se cierra el socket TCP subyacente, se liberan buffers de entrada/salida asociados.

Si hay operación de red en curso (solicitud HTTP enviada, esperando respuesta), la limpieza debe esperar que complete o la debe abortar. Abortar requiere cuidado: se cierra el socket, lo que causa que operación en curso experimente error de "connection lost". El handler de error debe tolerar este error esperado durante shutdown.

### 6.2 Limpieza de Credenciales y Secretos

Las credenciales de autenticación (usuario, contraseña) se almacenan en memoria durante operación. Durante limpieza, esta memoria debe ser sobrescrita con ceros antes de ser liberada, evitando que información sensible permanezca en memoria de proceso (vulnerable a ataques de leer memoria de proceso).

Práctica de seguridad recomendada es utilizar funciones especializadas de "secure erase" que sobreescriben memoria múltiples veces (patrones específicos de 0x00, 0xFF, patrones aleatorios) antes de liberación, previniendo que herramientas de análisis forense de memoria recuperen datos.

### 6.3 Cancelación de Threads de Red

Si se utiliza threading para operaciones asincrónicas de red (hilo separado que realiza solicitudes HTTP), durante limpieza se debe señalizar al hilo que se detenga, esperar que el hilo complete (típicamente usando join()), y luego liberar recursos del hilo.

Si el hilo no se detiene en tiempo razonable (timeout durante wait), se debe decidir si abortar limpieza o si dejar el hilo corriendo y aceptar fuga de hilo (menos deseable pero a veces necesario en situaciones de error extremo).

### 6.4 Descarga de Buffers Pendientes

Si había datos pendientes de envío al servidor (solicitudes de red no completadas, datos acumulados en buffers de salida), durante limpieza se puede elegir intentar enviar datos pendientes (para integridad transaccional), o descartar (si datos no son críticos). La elección depende de contexto operacional y requisitos de durabilidad.

### 6.5 Validación de Cierre Completo

El módulo web_sync debe reportar si cleanup completó exitosamente, si hubo condiciones de error durante cierre (error de red durante cierre, timeout esperando respuesta final), etc. Logs detallados de cierre permiten diagnóstico de problemas.

---

## Conclusión

El sistema de liberación de recursos implementado constituye infraestructura sofisticada que garantiza que cuando el plugin es descargado de OBS Studio, todos los recursos adquiridos se liberan completamente, evitando fugas de memoria, corrupción de estado del sistema anfitrión, y problemas de bloqueos de archivos que impedirían recarga posterior del plugin. La liberación de recursos de GPU requiere coordinación con contexto gráfico y sincronización con renderizado en curso; la liberación de texturas requiere gestión de caché y conteo de referencias; la limpieza de módulos especializados como detector ArUco y cliente web requiere entendimiento profundo de su estado interno y dependencias; y finalmente la liberación de estructuras de datos dinámicas requiere recursión cuidadosa a través de jerarquías de memoria. La implementación exhaustiva de validación durante cleanup (verificar que no hay recursos residuales, reportar condiciones de error) facilita diagnóstico rápido de bugs de gestión de memoria. En conjunto, estos mecanismos garantizan que el plugin mantiene comportamiento correcto durante múltiples cargas y descargas, siendo confiable para uso en producción de retransmisiones de larga duración donde la estabilidad del sistema es crítica para éxito operacional.
