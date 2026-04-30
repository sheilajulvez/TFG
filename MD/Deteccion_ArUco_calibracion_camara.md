# Detección ArUco y Calibración de Cámara

## Introducción

La precisión geométrica en sistemas de realidad aumentada depende fundamentalmente de la capacidad de localizar con exactitud marcadores visuales dentro del espacio tridimensional capturado por una cámara. El módulo de detección ArUco constituye el nexo crítico entre el mundo físico observado a través del dispositivo de captura de vídeo y la representación matemática de ese espacio que permite la síntesis de contenido virtual alineado correctamente con características físicas del entorno. El presente apartado se dedica al análisis exhaustivo de los mecanismos de detección de marcadores ArUco, las estrategias de calibración de cámara que garantizan precisión geométrica, y los algoritmos de transformación que convierten datos de detección brutos en estimaciones de pose (posición y orientación) en el espacio tridimensional. Se examina particularmente el módulo aruco_detector que actúa como interfaz C/C++ sobre las funcionalidades de OpenCV, los diferentes diccionarios de marcadores ArUco disponibles y sus implicaciones técnicas, los procesos de calibración de cámara que parametrizan la óptica específica de cada dispositivo de captura, y finalmente los algoritmos de transformación de coordenadas que proyectan información de detección desde el espacio de imagen bidimensional al espacio tridimensional del mundo.

## 1. Módulo aruco_detector: Interfaz de Abstracción para Detección de Marcadores

### 1.1 Arquitectura del Módulo y Envoltura de OpenCV

El módulo aruco_detector encapsula la funcionalidad de detección de marcadores ArUco proporcionada por OpenCV, exponiéndola a través de una interfaz C/C++ especializada definida en archivos aruco_detector.cpp y aruco_detector.h. Este módulo actúa como capa de abstracción que aísla el código específico del plugin de las dependencias directas sobre OpenCV, facilitando potenciales cambios futuros de biblioteca de visión por computadora o actualización de versiones de OpenCV sin necesidad de refactorización extensa del codebase.

La encapsulación es particularmente importante en contextos de plugins, donde la biblioteca anfitriona (OBS Studio) puede tener requisitos de compatibilidad específicos o preferencias sobre qué bibliotecas externas se utilizan. La interfaz expuesta por aruco_detector abstrae estos detalles, exponiendo únicamente funcionalidades necesarias para la funcionalidad específica del plugin, mientras oculta la complejidad de OpenCV.

El módulo implementa las siguientes funciones principales: `initialize_aruco_detector()` (inicialización con configuración específica), `detect_markers_in_frame()` (procesamiento de un fotograma para detectar marcadores), `get_marker_pose()` (extracción de posición y orientación de un marcador detectado), y `cleanup_aruco_detector()` (liberación de recursos). Estas funciones estructuran el ciclo de vida del detector de forma clara y predecible.

### 1.2 Proceso de Inicialización y Configuración de Marcadores

El proceso de inicialización del detector ArUco comienza con la selección e instantiación de un diccionario de marcadores (sección 1.3 detalla diccionarios específicos disponibles). El diccionario define el conjunto de patrones reconocibles como marcadores válidos; solo marcadores cuyo patrón corresponde exactamente a una entrada en el diccionario seleccionado serán detectados. Esta restricción es fundamental para seguridad y robustez: minimiza falsos positivos donde características no intencionales del entorno se interpretan incorrectamente como marcadores.

Seguidamente se configuran parámetros de detección que afectan el comportamiento del algoritmo: umbrales de confianza (valores mínimos de certeza para aceptar una detección como válida), tamaños mínimo y máximo de marcador (en píxeles) a buscar en la imagen, y parámetros de preprocesamiento de imagen como niveles de binarización. Estos parámetros son críticos para la robustez en diferentes condiciones de iluminación y tamaño de marcador.

La inicialización también establece parámetros de refinamiento de contornos, que aplicación de algoritmos adicionales post-detección para mejorar la precisión de las esquinas detectadas de los marcadores. El refinamiento es computacionalmente costoso pero mejora significativamente la precisión de estimación de pose, por lo que su habilitación típicamente depende de un balance entre requisitos de precisión y presupuesto computacional disponible en tiempo real.

Finalmente, se cargan los parámetros de calibración de cámara (sección 2) que serán necesarios para todas las operaciones posteriores de estimación de pose. Sin parámetros de calibración precisos, incluso detecciones perfectas de marcadores en el espacio 2D de imagen resultarían en estimaciones de pose 3D incorrectas.

## 2. Diccionarios ArUco: Especificación de Marcadores Reconocibles

### 2.1 Arquitectura y Variantes de Diccionarios

Un diccionario ArUco define un conjunto finito de patrones binarios que son reconocibles como marcadores válidos. Cada patrón consiste en una grilla de cuadrados blanco/negro que codifica un identificador único. La biblioteca OpenCV proporciona varios diccionarios predefinidos que varían en dos dimensiones: tamaño de la grilla (número de cuadrados por lado) y número total de marcadores disponibles en el diccionario.

Los diccionarios disponibles incluyen:

**Diccionario 4x4**: Cada marcador es una grilla de 4×4 = 16 cuadrados, con un borde blanco de calibración adicional. Este diccionario contiene hasta 50 marcadores únicos. El tamaño pequeño hace que los marcadores sean rápidos de detectar y requerentes de poco espacio físico, pero proporciona resolución limitada para detección de esquinas precisas.

**Diccionario 5x5**: Grilla de 5×5 = 25 cuadrados, con capacidad para 100 marcadores. Proporciona balance intermedio entre velocidad de detección y precisión de pose.

**Diccionario 6x6**: Grilla de 6×6 = 36 cuadrados, con capacidad para 250 marcadores. Proporciona mayor precisión de detección de esquinas a costo de mayor tamaño físico requerido.

**Diccionario 7x7**: Grilla de 7×7 = 49 cuadrados, con capacidad para 1000 marcadores. Proporciona máxima precisión pero requiere mayor espacio físico y tiempo de computación.

**Diccionario MIP (Múltiple Puntos de Interés)**: Diseño especializado que integra características adicionales para mejorar discriminabilidad entre marcadores, permitiendo mejor rechazo de falsos positivos en escenas complejas.

### 2.2 Selección de Diccionario y Tradeoffs de Diseño

La elección de diccionario involucra varios tradeoffs. Diccionarios pequeños (4x4) facilitan detección rápida y pueden utilizarse en marcadores pequeños, pero la baja resolución del patrón resulta en estimaciones de pose menos precisas. Diccionarios grandes (7x7) proporcionan mayor precisión pero requieren marcadores más grandes y tiempo de cómputo incrementado.

En el contexto de retransmisiones de concursos de programación, donde se desea máxima precisión de alineamiento (para que modelos virtuales se superponga exactamente sobre espacios físicos), típicamente se prefieren diccionarios más grandes (6x6 o 7x7), aceptando el costo de markers más grandes. Sin embargo, en escenarios con espacio limitado o donde múltiples marcadores deben coexistir en el mismo fotograma, diccionarios más pequeños pueden ser necesarios.

El algoritmo de detección ArUco es relativamente invariante al tamaño de diccionario en términos de robustez: un diccionario con mayor número de patrones distintos no es intrínsecamente menos confiable, aunque sí se reduce la distancia de Hamming mínima entre patrones (la diferencia entre patrón válido más cercano a una detección ruidosa y el patrón actual), lo que potencialmente reduce robustez ante ruido de imagen extremo.

## 3. Calibración de Cámara: Parametrización de Óptica y Distorsión

### 3.1 Fundamentos Matemáticos de Parámetros Intrínsecos

La calibración de cámara es el proceso de determinar parámetros que describen las propiedades ópticas de una cámara específica, permitiendo conversión precisa entre coordenadas de píxel en la imagen 2D y rayos 3D en el espacio del mundo. Estos parámetros se dividen en dos categorías: parámetros intrínsecos (propiedades ópticas de la cámara misma) y parámetros extrínsecos (posición y orientación de la cámara en el espacio del mundo, típicamente relativa a un objeto de referencia como un tablero de calibración).

Los parámetros intrínsecos incluyen la distancia focal (que caracteriza el ángulo de visión y magnificación), el punto principal (el punto en el plano de imagen hacia el cual apunta el eje óptico de la cámara), y coeficientes de distorsión (que modelan aberraciones ópticas donde líneas rectas en el mundo no corresponden a líneas rectas en la imagen).

Matemáticamente, la proyección de un punto 3D P = [X, Y, Z]T en el espacio del mundo a coordenadas de píxel [u, v]T se modela como:

$$\begin{bmatrix} u \\ v \\ 1 \end{bmatrix} = \frac{1}{Z} \begin{bmatrix} f_x & 0 & c_x \\ 0 & f_y & c_y \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} X \\ Y \\ Z \end{bmatrix}$$

donde fx y fy son distancias focales (típicamente medidas en píxeles), y cx, cy es el punto principal. Esta matriz 3×3 se denomina matriz de cámara intrínseca K.

### 3.2 Modelado de Distorsión Óptica

Las cámaras reales sufren distorsión óptica donde la relación lineal anterior no se mantiene exactamente. La distorsión se modela mediante polinomios de coeficientes de distorsión. El modelo de distorsión radial más común es:

$$u_{distorted} = u + k_1 r^2 + k_2 r^4 + k_3 r^6$$

$$v_{distorted} = v + k_1 r^2 + k_2 r^4 + k_3 r^6$$

donde r² = (u - cx)² + (v - cy)² es la distancia al cuadrado desde el punto principal, y k1, k2, k3 son coeficientes de distorsión radial. Adicionalmente existen coeficientes de distorsión tangencial p1, p2 que modelan distorsión asimétrica.

La corrección de distorsión es crítica para estimación de pose precisa: sin corrección, los errores de proyección se amplifican en la estimación de transformación 3D. OpenCV implementa algoritmos de undistortion que aplican inversa del modelo de distorsión, corrigiendo píxeles en la imagen para remover el efecto de distorsión óptica.

### 3.3 Proceso de Calibración y Almacenamiento de Parámetros

El proceso de calibración de cámara implica capturar imágenes de un patrón conocido (típicamente un tablero de ajedrez donde esquinas tienen posiciones 3D conocidas exactas), detectar las esquinas en múltiples imágenes, y resolver un sistema de ecuaciones no lineales para determinar parámetros que minimicen error de reproyección (diferencia entre posición estimada de esquina en imagen y posición real detectada).

Los parámetros de calibración resultantes se almacenan típicamente en archivos YAML o XML que especifican matriz de cámara K y vector de coeficientes de distorsión. El módulo aruco_detector carga estos archivos durante inicialización, permitiendo que diferentes cámaras (con diferentes ópticas y distorsiones) sean utilizadas sin necesidad de cambio de código. Esta flexibilidad es crítica en producciones de retransmisión donde diferentes cámaras podrían ser utilizadas en diferentes sesiones.

Un archivo de calibración típico contiene:

```yaml
camera_matrix: !!opencv-matrix
  rows: 3
  cols: 3
  dt: d
  data: [fx, 0, cx, 0, fy, cy, 0, 0, 1]
distortion_coefficients: !!opencv-matrix
  rows: 4
  cols: 1
  dt: d
  data: [k1, k2, p1, p2]
```

Estos parámetros permiten al sistema realizar undistortion de imágenes antes de procesamiento, asegurando que algoritmos de detección operen sobre imágenes geométricamente precisas.

## 4. Extracción de Transformaciones Espaciales: Rotación, Traslación, Proyección

### 4.1 Estimación de Pose mediante Algoritmos de Perspectiva-n-Puntos

Una vez que un marcador ArUco ha sido detectado y sus cuatro esquinas localizadas en coordenadas de píxel, el siguiente paso es determinar la posición y orientación tridimensionales del marcador respecto a la cámara. Este problema se denomina estimación de pose de perspectiva-n-puntos (PnP), donde las n esquinas del marcador proporcionan correspondencias conocidas entre puntos 3D (cuyas posiciones se conocen porque son esquinas de un patrón arUco estándar) y sus proyecciones 2D en la imagen (que se han detectado).

OpenCV implementa varios algoritmos PnP con diferentes características: EPNP (algoritmo rápido y robusto que es el default), ITERATIVE (refina iterativamente a través de múltiples iteraciones), y EPNP_REFINE (aplica iteraciones de refinamiento adicionales para mayor precisión).

El algoritmo PnP resuelve el sistema de ecuaciones:

$$\mathbf{p}_{2D} = K[R|t]\mathbf{P}_{3D}$$

donde p2D son coordenadas detectadas en píxeles, K es la matriz de cámara intrínseca, R es una matriz de rotación 3×3, t es un vector de traslación 3×1, y P3D son coordenadas 3D conocidas de las esquinas del marcador.

El solver PnP produce una estimación de la matriz de rotación R y vector de traslación t que mejor alinean los puntos 3D conocidos del marcador con sus proyecciones observadas en la imagen. Típicamente el resultado se verifica mediante cálculo de error de reproyección: se proyectan los puntos 3D utilizando R y t estimados, y se compara la posición proyectada con la posición detectada; diferencias grandes indican problemas en la detección o estimación.

### 4.2 Representación de Rotación: Vectores de Rotación a Ángulos de Euler

OpenCV expresa rotaciones internamente utilizando vectores de rotación (Rodrigues' rotation vector), un formato compacto donde la dirección del vector especifica el eje de rotación y la magnitud especifica el ángulo de rotación en radianes. Esta representación es matemáticamente elegante pero menos intuitiva que ángulos de Euler.

La conversión del vector de rotación a ángulos de Euler (pitch, yaw, roll) es necesaria para integración con el sistema de propiedades del plugin, que expresa rotaciones en ángulos de Euler para intuitividad del usuario. La conversión implica: (1) extraer ángulo de rotación como magnitud del vector de Rodrigues, (2) extraer eje de rotación como dirección normalizada del vector, (3) construir matriz de rotación equivalente utilizando la fórmula de Rodrigues, (4) descomponer la matriz de rotación en ángulos de Euler.

La descomposición de matriz de rotación a ángulos de Euler es singificativa porque existen múltiples formas de especificar una matriz de rotación mediante ángulos (dependiendo del orden de aplicación de rotaciones). El plugin típicamente utiliza orden ZYX (primero yaw alrededor de Z, luego pitch alrededor de Y, luego roll alrededor de X), con ángulos expresados en grados para conveniencia del usuario.

Matemáticamente, dada una matriz de rotación R, los ángulos de Euler se extraen mediante:

$$\text{pitch} = \arcsin(-R_{2,0})$$

$$\text{yaw} = \arctan2(R_{1,0}, R_{0,0})$$

$$\text{roll} = \arctan2(R_{2,1}, R_{2,2})$$

donde Ri,j denota el elemento en fila i, columna j de la matriz de rotación.

### 4.3 Detección Basada en Identificador de Marcador

Los marcadores ArUco llevan identificadores únicos (IDs) codificados en su patrón binario. El algoritmo de detección extrae no solo la posición de esquinas, sino también decodifica el patrón binario para extraer el ID del marcador. Este ID permite correlacionar marcadores detectados en fotogramas sucesivos, facilitando seguimiento temporal y detección de cuando marcadores entran o salen de vista.

La detección basada en ID es particularmente útil en escenarios donde múltiples marcadores pueden estar presentes simultáneamente, pero solo uno es de interés para un objetivo específico (por ejemplo, en una competencia de programación, un marcador con ID específico podría indicar la zona de proyección principal donde se desea superponer un modelo 3D específico). El plugin puede configurarse para detectar específicamente un ID o rango de IDs, ignorando otros marcadores.

La extracción de ID involucra primero extraer la región central del patrón (excluyendo el borde blanco de calibración), convertir esa región a un vector binario, y buscar en el diccionario de marcadores para encontrar el patrón coincidente más cercano. El "más cercano" se mide mediante distancia de Hamming (número de bits que difieren), permitiendo cierta robustez ante ruido de imagen.

### 4.4 Proyección de Pose a Coordenadas de Pantalla

Para aplicaciones de visualización, es necesario proyectar la estimación de pose 3D de vuelta a coordenadas 2D de pantalla, determinando donde en la pantalla debe renderizarse información visual sobre el marcador detectado. Este proceso es la inversa del problema de estimación de pose: dada la matriz de transformación (R, t) y una posición de interés en el espacio del marcador (por ejemplo, el centro del marcador), calcular donde aparece en la imagen.

La proyección se realiza mediante:

$$\mathbf{p}_{pixel} = K(R\mathbf{P}_{local} + t)$$

donde Plocal es un punto en coordenadas locales del marcador (por ejemplo, [0, 0, 0] para el centro), K es la matriz de cámara intrínseca, y ppixel es la posición resultante en píxeles.

Esta proyección es utilizada para renderizar overlays informativos, tales como indicadores visuales de estado de detección, información de pose estimada, o anotaciones ubicadas cerca del marcador detectado. La precisión de proyección es importante para que anotaciones visuales aparezcan en posiciones visualmente coherentes.

## 5. Integración del Módulo ArUco en el Ciclo de Ejecución del Plugin

### 5.1 Flujo de Procesamiento Frame-a-Frame

El módulo aruco_detector se integra en el ciclo de ejecución del plugin de forma coordinada. Durante la etapa de inicialización (filter_create), el detector se instancia con el diccionario y parámetros especificados. Durante la etapa de tick (filter_tick), para cada fotograma nuevo: (1) se extrae el fotograma de entrada, (2) se aplica undistortion de imagen utilizando parámetros de calibración, (3) se invoca el detector ArUco para identificar marcadores presentes, (4) para cada marcador detectado, se estima su pose, (5) se almacenan resultados (IDs detectados, poses estimadas) en el estado del plugin para uso posterior en renderizado.

La separación entre detección (en tick) y renderizado (en render) facilita eficiencia: la detección es computacionalmente costosa, mientras que renderizado es relativamente ligero. Esta separación permite que el costo computacional se distribuya de forma más pareja.

### 5.2 Parámetros Configurables y Recalibración Dinámica

El módulo expone varios parámetros configurables a través del sistema de propiedades de OBS: archivo de calibración de cámara a utilizar, diccionario ArUco a seleccionar, umbrales de confianza de detección, tamaños mínimo y máximo de marcador. Cuando el usuario modifica la propiedad de archivo de calibración, el nuevo archivo se carga y sus parámetros se aplican a todas las detecciones posteriores.

La selección de diccionario es también reconfigurable, permitiendo cambio dinámico sin necesidad de reiniciar el plugin. Esto es particularmente útil en producciones donde se desea experimentar con diferentes tamaños de marcador o cantidad de marcadores soportados.

### 5.3 Robustez ante Fallos de Detección y Estrategias de Fallback

En la práctica, la detección de marcadores no siempre tiene éxito: iluminación insuficiente, marcadores parcialmente obscurecidos, o artefactos de compresión de vídeo pueden resultar en fallos de detección. El sistema implementa estrategias de robustez: si un marcador esperado no se detecta en un fotograma, se intenta utilizar la última estimación de pose conocida del fotograma anterior, suavizando visualmente la transición durante pérdidas transitorias de seguimiento.

Alternativamente, se pueden mantener modelos de predicción temporal (tales como filtros de Kalman) que predicen donde debería aparecer el marcador en el fotograma actual basándose en su trayectoria en fotogramas anteriores, facilitando que el detector se enfoque en regiones más pequeñas de búsqueda y sea más robusto ante cambios visuales.

---

## Conclusión

El sistema de detección ArUco y calibración de cámara implementado representa la integración sofisticada de algoritmos de visión por computadora maduros (proporcionados por OpenCV) con una arquitectura de plugin especializada. La encapsulación mediante el módulo aruco_detector permite abstraer complejidad de OpenCV, exposición selectiva de funcionalidad necesaria, y potencial evolución tecnológica sin refactorización extensiva. La disponibilidad de múltiples diccionarios de marcadores, combinada con estrategias de calibración flexible mediante archivos YAML, garantiza que el sistema pueda adaptarse a diferentes escenarios de producción, diferentes cámaras, y diferentes requisitos de precisión versus velocidad. Los algoritmos de estimación de pose permiten localización tridimensional precisa de marcadores, habilitando el alineamiento correcto de contenido virtual con características físicas del entorno. En conjunto, estos componentes forman la columna vertebral de la funcionalidad de realidad aumentada, transformando datos brutos de cámara en estimaciones de pose que permiten la síntesis de experiencias visuales coherentes donde lo virtual y lo físico se entrelazan visualmente de forma convincente.
