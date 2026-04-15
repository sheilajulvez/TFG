# Carga y Gestión de Modelos 3D

## Introducción

La visualización de contenido tridimensional en tiempo real constituye uno de los desafíos técnicos más complejos en el contexto de retransmisiones de realidad aumentada para concursos de programación. La capacidad de cargar, manipular y renderizar modelos geométricos tridimensionales de forma eficiente, mientras se mantiene una tasa de fotogramas estable y se garantiza precisión en el alineamiento con marcadores detectados en el espacio físico, requiere una arquitectura robusta de gestión de recursos gráficos. El presente apartado se dedica al análisis exhaustivo de los mecanismos implementados para la carga y gestión de modelos 3D, incluyendo la estructura de datos que representa geometría poligonal, los algoritmos de transformación espacial que posicionan modelos en coordenadas del mundo virtual, y los sistemas de texturización que aportan realismo visual a la geometría abstracta. Se examina también cómo el sistema gestiona la complejidad inherente a modelos constituidos por múltiples mallas (meshes), cómo se coordinan rotaciones y escalados manteniendo coherencia espacial, y cómo se integran sistemas de visualización de texto 3D y overlays informativos que enriquecen la experiencia visual de la audiencia.

## 1. Módulo SJ_3DModel: Carga y Renderizado de Geometría Poligonal

### 1.1 Arquitectura del Módulo de Gestión 3D

El módulo SJ_3DModel constituye la capa de abstracción fundamental para todas las operaciones relacionadas con geometría tridimensional. Este módulo se define típicamente en archivos SJ_3DModel.c y SJ_3DModel.h, implementando funciones de carga de archivos, validación de geometría, y funciones de renderizado que interactúan con la API gráfica subyacente (OpenGL, DirectX, o Metal dependiendo de la plataforma).

El módulo implementa parsers para el formato OBJ (Wavefront OBJ), un formato de texto ampliamente soportado que representa geometría poligonal mediante la especificación de vértices, caras (faces), coordenadas de textura (texture coordinates), y vectores normales. La elección del formato OBJ se justifica por su simplicidad, portabilidad, y amplio soporte en herramientas de modelado 3D profesionales, permitiendo que artistas y desarrolladores trabajen con sus herramientas preferidas sin requerir conversiones de formato complejo.

El proceso de carga de un archivo OBJ implica varias etapas secuenciales: lectura del archivo de texto línea por línea, parseo de cada instrucción (línea que comienza con un prefijo específico como 'v' para vértice, 'vt' para coordenada de textura, 'f' para cara), validación de sintaxis, y almacenamiento de datos extraídos en estructuras de datos internas. La validación de sintaxis durante el parseo permite detectar tempranamente archivos corruptos o malformados, registrando errores específicos que facilitan el diagnóstico de problemas.

### 1.2 Estructura de Datos de Malla (Mesh): Representación de Geometría

Una malla (mesh) es la estructura de datos fundamental que representa una colección de triángulos poligonales en el espacio 3D. La estructura típica de malla contiene tres componentes principales interdependientes:

El primer componente es el array de vértices (vertex buffer), que almacena las coordenadas tridimensionales (x, y, z) de cada punto que define la geometría. Cada vértice se representa como un punto en el espacio euclidiano tridimensional, típicamente almacenado como tres valores de punto flotante de precisión simple (float). Un modelo 3D complejo puede contener decenas de miles de vértices, por lo que la gestión eficiente de memoria es crítica. Los vértices se almacenan típicamente en memoria GPU como buffers de vértices (Vertex Buffer Objects en OpenGL), permitiendo acceso rápido durante el renderizado.

El segundo componente es el array de índices (index buffer), que especifica cómo se conectan los vértices para formar triángulos. En lugar de especificar explícitamente tres vértices para cada triángulo, se especifica el índice (posición) de cada vértice en el array de vértices. Esta representación reduce significativamente el uso de memoria cuando vértices son compartidos entre múltiples triángulos (lo que es típico en cualquier geometría no trivial), evitando duplicación de datos de vértices.

El tercer componente abarca datos de atributos de vértice adicionales, incluyendo coordenadas de textura (texture coordinates o UVs) que especifican cómo proyectar una textura 2D sobre la superficie 3D del modelo, y vectores normales que especifican la dirección perpendicular a cada cara poligonal. Estos vectores normales son esenciales para cálculos de iluminación realista, determinando cómo la luz interactúa con cada superficie.

La estructura de malla contiene además metadatos tales como el número total de vértices, el número total de índices, referencias a materiales (que especifican texturas y propiedades de superficie), y referencias a buffers de GPU donde se almacenan los datos geométricos para acceso rápido durante renderizado. Opcionalmente, la estructura puede incluir información de bounding box (cuadro delimitador que envuelve toda la geometría), útil para optimizaciones de culling (determinación de qué geometría es visible y debe ser renderizada).

### 1.3 Manejo de Múltiples Mallas en Modelos Complejos

Un modelo 3D complejo no necesariamente consiste en una única malla. En su lugar, modelos complejos se descomponen típicamente en múltiples mallas, cada una representando un subcomponente visual. Esta descomposición facilita la gestión: diferentes mallas pueden tener diferentes materiales y texturas, pueden ser ocultadas o mostradas independientemente, y pueden ser renderizadas en diferentes órdenes para efectos visuales específicos.

El módulo SJ_3DModel mantiene una colección de mallas (típicamente un array o estructura de lista dinámica) que pertenecen a un único modelo. Cuando se realiza el renderizado del modelo, se itera sobre todas las mallas, renderizando cada una con sus parámetros visuales específicos. Esta arquitectura facilita también la implementación de niveles de detalle (LOD - Level of Detail), donde modelos complejos pueden utilizar diferentes números de mallas dependiendo de la distancia a la cámara: a distancias cercanas se renderizan todas las mallas con máximo detalle, mientras que a distancias lejanas se pueden omitir mallas de detalle menor, reduciendo carga computacional.

La gestión de múltiples mallas requiere también coordinación de transformaciones: si se desea rotar el modelo completo, todas las mallas deben rotarse utilizando las mismas matrices de transformación. Esta coordinación se implementa típicamente a nivel del modelo (no a nivel de malla individual), donde transformaciones se aplican una sola vez y se propagan a todas las mallas subordinadas.

## 2. Transformaciones Espaciales: Rotación, Escalado y Posicionamiento

### 2.1 Matemática de Transformaciones: Matrices Homogéneas

Las transformaciones geométricas (rotación, escalado, traslación) se implementan mediante álgebra matricial, específicamente mediante matrices de transformación homogéneas de tamaño 4×4. Estas matrices permiten representar de forma unificada todas las transformaciones afines, permitiendo composición eficiente de múltiples transformaciones mediante multiplicación matricial.

Una matriz de transformación homogénea adopta la forma:

$$\begin{bmatrix} r_{00} & r_{01} & r_{02} & t_x \\ r_{10} & r_{11} & r_{12} & t_y \\ r_{20} & r_{21} & r_{22} & t_z \\ 0 & 0 & 0 & 1 \end{bmatrix}$$

donde la submatriz superior izquierda 3×3 (rij) representa la combinación de rotación y escalado, mientras que la columna de la derecha (tx, ty, tz) representa la traslación. Esta formulación permite que una transformación compuesta (primero rotación, luego escalado, luego traslación) se represente como el producto de matrices individuales, evitando la necesidad de aplicar transformaciones secuencialmente.

### 2.2 Rotación de Modelos: Ángulos de Euler y Cuaterniones

La rotación de modelos 3D se especifica típicamente mediante ángulos de Euler (pitch, yaw, roll), representando rotaciones alrededor de los ejes X, Y, Z respectivamente. Estos ángulos son intuitivos para usuarios finales, quienes pueden pensar en "girar alrededor del eje vertical" (yaw) o "inclinar hacia adelante/atrás" (pitch).

Sin embargo, la representación mediante ángulos de Euler presenta limitaciones matemáticas. La interpolación entre dos rotaciones especificadas en ángulos de Euler no siempre produce rotaciones suaves intermedias, y existe el fenómeno de gimbal lock (bloqueo de cardán), donde ciertos ángulos resultan en pérdida de un grado de libertad de rotación. Por estas razones, internamente el sistema puede convertir ángulos de Euler a cuaterniones para cálculos, aprovechando las propiedades matemáticas superiores de los cuaterniones para interpolación y composición de rotaciones.

Un cuaternión es una extensión cuatridimensional de los números complejos, representado como q = (w, x, y, z), que puede codificar cualquier rotación 3D de forma única. La conversión entre ángulos de Euler y cuaterniones es biyectiva (con excepciones en casos singulares), permitiendo que el sistema utilice la representación más conveniente para cada contexto: ángulos de Euler para especificación del usuario y almacenamiento, cuaterniones para cálculos internos.

### 2.3 Escalado y Cálculo de Centros de Rotación

El escalado de modelos se especifica mediante un factor de escala global, que multiplica uniformemente todas las dimensiones del modelo. Escalado no uniforme (factores diferentes para cada eje) podría implementarse alternativamente, pero la escala uniforme es más intuitiva y evita distorsiones no deseadas de la geometría.

El cálculo del centro de rotación (pivot point) es crítico para que las rotaciones sean visualmente correctas. Por defecto, las rotaciones se realizan alrededor del origen (0, 0, 0) del sistema de coordenadas del modelo. Sin embargo, en muchos casos es deseable rotar alrededor de un punto diferente (por ejemplo, el centro geométrico del modelo, o la base de una figura humanoides). El módulo calcula automáticamente el bounding box del modelo (el cuadro rectangular más pequeño que envuelve toda la geometría) y utiliza su centro como pivot point por defecto, aunque este puede ser anulado por parámetros de usuario.

Matemáticamente, para rotar un punto P alrededor de un pivot point C, se ejecutan los siguientes pasos: (1) traslación de P por -C, moviendo el pivot al origen, (2) aplicación de la rotación R, y (3) traslación por +C, moviendo el resultado de vuelta a su posición relativa al pivot original. Esta secuencia se representa como la multiplicación de tres matrices: T(C) × R × T(-C) × P.

## 3. Propiedades Configurables y Control del Usuario

### 3.1 Interfaz de Propiedades: Parámetros de Modelo

El módulo SJ_3DModel expone un conjunto de propiedades configurables que permiten a usuarios finales (operadores de retransmisión) controlar el comportamiento del renderizado 3D sin necesidad de modificar código o recompilar. Estas propiedades se integran con el sistema de propiedades de OBS Studio, permitiendo que valores de propiedades se persistan en archivos de configuración y se modifiquen a través de la interfaz gráfica.

**Selección de archivo de modelo (OBJ)**: Esta propiedad especifica la ruta al archivo OBJ que debe cargarse. Cuando el usuario cambia esta propiedad, el sistema descarga el modelo anterior (liberando memoria y recursos GPU) y carga el nuevo modelo. La validación asegura que la ruta existe y que el archivo es un archivo OBJ válido. El selector de archivos típicamente utiliza diálogos de navegación de archivos del sistema operativo, facilitando la exploración del sistema de archivos sin necesidad de escribir manualmente rutas.

**Selección de textura personalizada**: Esta propiedad especifica la ruta a una imagen (típicamente en formato PNG, JPG, o TGA) que será utilizada como textura primaria del modelo. Un modelo OBJ puede especificar una textura por defecto en su archivo, pero esta propiedad permite al usuario anular esa selección. La carga de textura implica leer el archivo de imagen, validar su formato, crear una textura GPU a partir de los datos de píxeles, y configurar parámetros de textura (wrapping, filtering) que determinan cómo se aplica la textura a la geometría.

**Control de escala global**: Un parámetro numérico que especifica un factor de escala multiplicativo. Valores mayores a 1.0 amplifican el modelo, mientras que valores menores a 1.0 lo reducen. Este parámetro es de particular importancia en contextos de realidad aumentada, donde el tamaño visual del modelo virtual debe corresponderse de forma sensata con el tamaño del marcador físico sobre el cual se superpone.

**Posicionamiento manual (X, Y, Z)**: Tres parámetros numéricos que especifican desplazamientos aditivos del modelo en las tres dimensiones espaciales. Estos offsets se aplican después de que el modelo ha sido posicionado según la detección de marcadores ArUco, permitiendo ajustes finos de alineamiento. Los valores típicamente se especifican en unidades equivalentes a unidades de espacio de modelo 3D, aunque pueden escalarse según la distancia a la cámara para mantener proporciones visuales consistentes.

**Rotación manual (Pitch, Yaw, Roll)**: Tres parámetros que especifican rotaciones adicionales alrededor de ejes X, Y, Z respectivamente. Estos ángulos se especifican típicamente en grados, permitiendo valores en el rango [0, 360) o [-180, 180) dependiendo de la convención. Estos offsets de rotación se aplican composicionalmente con la rotación base derivada de la detección de marcadores ArUco, permitiendo correcciones visuales cuando existe desalineamiento sistemático.

### 3.2 Aplicación Dinámica de Texturas y Re-renderizado

Cuando el usuario modifica la propiedad de selección de textura, el sistema debe responder dinámicamente sin interrumpir la retransmisión. El proceso es: (1) cargar la nueva textura de imagen en memoria CPU, (2) validar que tiene dimensiones y formato compatibles, (3) crear una nueva textura GPU, (4) actualizar referencias en la estructura de malla para que apunte a la nueva textura, (5) liberar la textura GPU anterior. Este proceso ocurre típicamente en una etapa de update separada del renderizado principal, garantizando que cambios de textura no causen tartamudeos visuales.

La validación de textura incluye verificación de dimensiones (típicamente potencias de 2 para compatibilidad óptima con hardware GPU antiguo, aunque hardware moderno soporta dimensiones arbitrarias), validación de formato de píxel (RGB, RGBA, etc.), y comprobación de que la textura no es excesivamente grande (lo que consumiría memoria GPU limitada).

## 4. Sistema de Renderizado de Texto 3D y Overlays Informativos

### 4.1 Renderizado de Texto en Espacio 3D

El renderizado de texto 3D constituye un desafío técnico significativo en sistemas de gráficos. A diferencia del texto 2D tradicional que se renderiza en la pantalla después de la proyección, el texto 3D debe ser posicionado en el espacio 3D junto con otros objetos, experimentando transformaciones de perspectiva y oclusión de forma consistente con la geometría 3D circundante.

El enfoque implementado utiliza fuentes rasterizadas (bitmaps de caracteres precompilados) que se convierten en mallas 3D. Cada carácter de la fuente se renderiza como una malla planar (un rectángulo planar con la imagen del carácter proyectada mediante un sampler de textura). Las mallas de caracteres se posicionan secuencialmente a lo largo de una línea de base, creando la ilusión de texto legible en 3D.

El módulo de renderizado de texto 3D expone funciones como `render_3d_text(const char *text, vec3 position, quat rotation, float scale)`, que aceptan el texto a renderizar, su posición 3D, orientación (como cuaternión), y escala. Internamente, la función genera una malla temporal para el texto, aplica las transformaciones especificadas, y renderiza la malla como parte del pipeline gráfico normal.

### 4.2 Proyección de Texto Sincronizado con Modelos

La proyección de texto en el espacio 3D debe mantener sincronización visual con otros elementos. En particular, cuando texto se renderiza junto con un modelo principal, es común desear que el texto sea sincronizado con la posición y rotación del modelo, comportándose como si estuviera adherido a la superficie del modelo.

Para lograr esto, el sistema aplica las mismas transformaciones de posición y rotación del modelo al texto, garantizando que ambos experimentan movimientos idénticos. Si el modelo se desplaza por una traslación de vector T, el texto también se desplaza por T. Si el modelo se rota alrededor de un pivot point C, el texto también se rota alrededor del mismo pivot.

La transformación de coordenadas se realiza mediante matrices de transformación: la matriz de modelo (que combina escala, rotación y traslación) se multiplica por las coordenadas de posición del texto en el espacio del modelo local, produciendo coordenadas en el espacio del mundo. Estas coordenadas mundiales se proyectan entonces al espacio de pantalla 2D utilizando matrices de visualización y proyección de la cámara.

### 4.3 Overlays de Información y Metadatos de Sistema

Los overlays representan capas de información visual que se superponen sobre el contenido principal de renderizado. A diferencia del texto 3D que existe en el espacio 3D y experimenta perspectiva, los overlays típicamente se renderizan en el espacio 2D de pantalla, apareciendo siempre en ubicaciones fijas independientemente de la perspectiva de cámara 3D.

Un overlay de información de calibración y parámetros activos renderiza datos técnicos útiles para diagnosticar el estado del sistema: matriz de calibración actual de la cámara (si se desea visualizar parámetros intrínsecos), parámetros activos del detector ArUco (umbrales de confianza, tamaños de marcador), información de pose estimada (posición y rotación del marcador en coordenadas mundiales), y métricas de rendimiento (tasa de fotogramas actual, tiempo de procesamiento de detección).

Un overlay de estado de detección AR especifica visualmente si el sistema ha detectado exitosamente marcadores en el fotograma actual: si la detección es exitosa, un indicador visual (típicamente de color verde) se renderiza en una esquina de la pantalla, junto con identificadores de marcadores detectados. Si la detección falla, un indicador rojo aparece, y opcionalmente se despliega información diagnóstica sugiriendo razones del fallo (insuficiente iluminación, marcador fuera de vista, etc.).

### 4.4 Configuración de Offset Visual para Overlays

Los overlays pueden posicionarse flexiblemente dentro del espacio de pantalla 2D. Parámetros de offset configurables permiten especificar la posición de cada overlay (por ejemplo, esquina superior izquierda, esquina inferior derecha, centro, etc.), el tamaño de fuente, el color de texto, y el nivel de opacidad (alpha blending). Estos parámetros se exponen como propiedades configurables, permitiendo a operadores de retransmisión personalizar la apariencia de overlays para adaptarse a su marca visual específica o preferencias estéticas.

El sistema aplica transformaciones de offset a la posición base del overlay, permitiendo ajustes finos de posicionamiento sin requerir modificación de código. Un offset configurable típicamente consiste en dos componentes: un offset de posición (en píxeles o unidades normalizadas de pantalla) y un offset angular (si el overlay debe rotarse, aunque esto es menos común para texto).

---

## 5. Integración Holística: Orquestación de Componentes Geométricos y Visuales

La integración de módulos de geometría 3D, transformaciones espaciales, gestión de propiedades, y sistemas de renderizado de texto requiere un diseño arquitectónico que garantice consistencia y eficiencia. El flujo de ejecución durante cada fotograma es el siguiente:

En primer lugar, se actualizan las propiedades de usuario si alguna ha cambiado en la etapa anterior (frame anterior). Si la propiedad de modelo ha cambiado, se descarga el modelo anterior y se carga el nuevo. Si la propiedad de escala ha cambiado, se recalcula la matriz de escalado. Estas operaciones de actualización se ejecutan típicamente en la función filter_update() de OBS, separadas del renderizado tiempo real.

Seguidamente, en la etapa de tick (filter_tick()), se actualiza el estado del sistema: se obtienen estimaciones de pose de marcadores ArUco (posición y rotación), se aplican offsets de posición y rotación especificados por usuario, y se construyen matrices de transformación completas que codifican escala, rotación y traslación.

Finalmente, en la etapa de renderizado (filter_render()), el sistema genera la salida visual: (1) se configura el contexto gráfico GPU, (2) para cada malla del modelo, se aplica su matriz de transformación, se vinculan sus buffers de vértices e índices, se asigna su textura, y se ejecuta el comando de renderizado; (3) se renderizan textos 3D en sus posiciones transformadas; (4) se renderizan overlays de información en coordenadas de pantalla 2D fijas.

Esta orquestación garantiza que todos los componentes visuales experimentan transformaciones consistentes, permitiendo que usuarios finales observen una composición visual coherente donde modelos 3D virtuales, texto anotativo, e información de sistema aparecen en posiciones sensatas y visualmente consistentes con respecto a los marcadores físicos detectados en el espacio real capturado por la cámara.

---

**Conclusión**

El sistema de carga y gestión de modelos 3D implementado constituye una arquitectura sofisticada que balancea eficiencia computacional con flexibilidad de configuración. La separación clara entre la especificación de geometría (archivos OBJ), la aplicación de propiedades visuales (texturas, escalado), y la gestión de transformaciones espaciales, permite que cada componente evolucione independientemente mientras se mantiene coherencia visual global. La integración de sistemas de texto 3D y overlays informativos enriquece la experiencia comunicativa, permitiendo que retransmisiones de concursos de programación no solamente presenten modelos virtuales visualmente atractivos, sino que también proporcionen contexto técnico y información de estado que facilita la comprensión por parte de audiencias remotas.
