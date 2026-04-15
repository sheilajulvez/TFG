# Propiedades de Modo AR

## Introducción

La flexibilidad operacional de sistemas de realidad aumentada en contextos de retransmisión en directo depende críticamente de la disponibilidad de parámetros configurables que permitan adaptación a diferentes escenarios de producción, características de hardware heterogéneas, y requisitos específicos de cada evento de programación. El modo de realidad aumentada (AR) del plugin implementa un conjunto sofisticado de propiedades que habilitan la personalización exhaustiva del comportamiento de detección, seguimiento y renderizado de contenido virtual superpuesto. El presente apartado se dedica al análisis detallado de estas propiedades configurables, examinando cómo cada parámetro afecta el comportamiento del sistema, las interdependencias entre propiedades, las restricciones de validación que garantizan coherencia operacional, y las implicaciones técnicas de diferentes valores de configuración. Se analiza particularmente la selección de diccionarios ArUco y su impacto en capacidad de detección, la especificación del identificador de marcador que actúa como selector de objetivo específico, el tamaño físico del marcador que es crítico para estimación de pose precisa, la ruta de archivo de calibración de cámara que parametriza la óptica del dispositivo de captura, y finalmente los offsets de posición y rotación que permiten correcciones visuales finas y adaptaciones a desalineamientos sistemáticos.

## 1. Selección de Diccionario ArUco y Configuración de Patrones Reconocibles

### 1.1 Propiedades de Diccionario y Implicaciones Operacionales

La propiedad de selección de diccionario ArUco constituye un parámetro crítico que determina el conjunto de patrones binarios que el sistema reconocerá como marcadores válidos. La disponibilidad de múltiples diccionarios predefinidos en OpenCV (4x4, 5x5, 6x6, 7x7, MIP) permite que operadores de retransmisión seleccionen el diccionario más apropriado para su contexto operacional específico, balanceando requisitos competitivos entre precisión de detección, velocidad de procesamiento, y limitaciones de espacio físico.

La selección del diccionario es una decisión de diseño crítica que afecta múltiples aspectos del sistema downstream. La selección de un diccionario pequeño (4x4) resulta en detecciones rápidas y bajo uso de memoria, pero limita el número de marcadores únicos disponibles (máximo 50) y reduce la precisión de estimación de pose debido a la baja resolución del patrón binario. Inversamente, la selección de un diccionario grande (7x7) proporciona 1000 marcadores únicos y máxima precisión de pose, pero requiere marcadores más grandes físicamente, incrementa tiempo de procesamiento, y consume más memoria en almacenamiento de parámetros de diccionario.

El diccionario MIP (Multiple Independent Patterns) representa una alternativa especializada que integra características de discriminación mejorada, permitiendo mejor rechazo de falsos positivos en escenas visuales complejas donde patrones no intencionales podrían interpretarse como marcadores. La selección de diccionario MIP es particularmente apropiada en producciones con fondo visual complejo o donde confiabilidad de detección es crítica para experiencia de usuario.

### 1.2 Integración con Pipeline de Detección y Efectos en Robustez

Una vez seleccionado, el diccionario se instancia en memoria durante la inicialización del plugin, permaneciendo constante durante toda la sesión de retransmisión. La selección de diccionario se propaga directamente al módulo aruco_detector (sección 4 del apartado anterior), donde parametriza el comportamiento del algoritmo de detección. El cambio dinámico de diccionario requiere reinicialización completa del detector, lo que implica potencial interrupción transoria de detección mientras el nuevo diccionario se carga y valida.

El impacto en robustez es significativo: la distancia de Hamming mínima entre patrones válidos en el diccionario determina la tolerancia a ruido de imagen. Diccionarios con mayor número de patrones distintos típicamente tienen distancia de Hamming mínima reducida, lo que potencialmente reduce robustez ante artefactos de compresión de vídeo o degradación de imagen. Esta tradeoff debe considerarse cuidadosamente en contextos donde la fuente de vídeo es comprimida mediante códecs como H.264 o H.265, que pueden introducir artefactos que simulan cambios de bits en el patrón del marcador.

## 2. Identificador de Marcador: Especificación de Objetivo de Detección

### 2.1 Mecanismo de Selección por ID y Ventajas de Especificidad

La propiedad de identificador de marcador (ID) actúa como filtro que especifica qué marcador particular será objeto de detección y seguimiento en modo AR. En lugar de detectar todos los marcadores presentes en el fotograma (lo que es la conducta por defecto), la especificación de un ID particular restringe el sistema a detectar únicamente marcadores cuyo patrón binario decodifica al ID especificado, ignorando otros marcadores que podrían estar presentes simultáneamente.

Este mecanismo de selección por ID proporciona múltiples ventajas operacionales. En primer lugar, reduce la carga computacional de detección: en lugar de procesar todos los marcadores encontrados, el sistema puede enfocarse en búsqueda de un patrón específico. En segundo lugar, en escenarios donde múltiples marcadores están presentes en el mismo espacio físico (por ejemplo, en un estudio de retransmisión con múltiples zonas de captura), permite que cada zona renderice contenido virtual independiente al establecer diferentes IDs para cada zona.

### 1.2 Rango de IDs Válidos y Esquemas de Asignación

El rango válido de IDs depende del diccionario seleccionado: diccionarios 4x4 soportan IDs 0-49, diccionarios 5x5 soportan 0-99, diccionarios 6x6 soportan 0-249, y diccionarios 7x7 soportan 0-999. La selección de un ID fuera del rango válido para el diccionario actualmente instanciado resultará en fallo de detección: el sistema buscará un patrón que no existe en el diccionario.

Para evitar errores de especificación, es recomendable que esquemas de identificación de marcadores sigan convenciones claras: por ejemplo, IDs 0-9 podrían reservarse para marcadores de zona principal, IDs 10-19 para marcadores secundarios, IDs 20-29 para marcadores de calibración. Esta organización facilita el entrenamiento de operadores de retransmisión y reduce probabilidad de errores de configuración.

La propiedad de ID es típicamente inmutable durante operación en vivo (cambiar el ID requeriría reconfiguración manual), por lo que su selección debe realizarse antes de iniciar retransmisión. Sin embargo, el sistema puede soportar selección dinámica de ID si se implementa la lógica correspondiente, permitiendo cambio de objetivo de detección durante retransmisión en vivo sin reinicialización.

## 3. Tamaño del Marcador: Especificación de Dimensiones Físicas y Estimación de Escala

### 3.1 Rol del Tamaño en Estimación de Pose Precisa

El parámetro de tamaño de marcador especifica las dimensiones físicas (en metros) del marcador detectado en el espacio real. Este parámetro es crítico para estimación de pose precisa, ya que el algoritmo de perspectiva-n-puntos (PnP) requiere conocimiento exacto de las dimensiones 3D del objeto de referencia (en este caso, el marcador) para poder estimar la transformación (posición y orientación) de forma correcta.

Matemáticamente, la estimación de pose resuelve para la matriz de transformación [R|t] que relaciona coordenadas 3D del marcador (especificadas en metros según el tamaño del marcador) con sus proyecciones detectadas en píxeles. Si el tamaño especificado no corresponde con el tamaño físico real, la transformación estimada será incorrecta: el marcador podrá aparecer escalado (pareciendo más grande o más pequeño de lo que debería), desplazado, o rotado de forma sistemática.

El impacto en experiencia visual es significativo: si el tamaño especificado es mayor que el tamaño real, los modelos virtuales superpuestos aparecerán más lejanos que el marcador físico (profundidad incorrecta), desalojándose visualmente de las posiciones esperadas. Si el tamaño especificado es más pequeño, los modelos aparecerán más cercanos, nuevamente resultando en desalineamiento visual.

### 3.2 Precisión de Medición y Calibración de Tamaño

La especificación precisa del tamaño requiere medición física cuidadosa del marcador impreso. Típicamente, se mide la distancia entre bordes exteriores del patrón de grilla blanco/negro (excluyendo el borde blanco de calibración), especificándose en metros. Un error de medición de 1-2 milímetros en un marcador de 10 centímetros representa error relativo de 1-2%, que puede ser tolerable en algunas aplicaciones pero problemático en contextos que requieren alineamiento precisio como retransmisiones de concursos de programación.

Para garantizar consistencia, es recomendable especificar el tamaño teórico esperado basado en el diseño del marcador, en lugar de mediciones empíricas que podrían introducir variabilidad. Alternativamente, el sistema puede soportar calibración de tamaño dinámico donde una rutina especial presenta el marcador en múltiples poses y distancias conocidas, permitiendo al sistema determinar el tamaño que minimiza error de reproyección.

### 3.3 Relación entre Tamaño y Rango de Detección Efectivo

El tamaño físico del marcador afecta también el rango de distancias a la cámara en el cual detección confiable es posible. Marcadores muy pequeños (por ejemplo, 2 cm) solo pueden detectarse cuando están relativamente cercanos a la cámara, ya que a distancias mayores su tamaño en píxeles se reduce, resultando en imagen de baja resolución donde características finas del patrón se pierden por límites de resolución de píxel.

La relación es aproximadamente logarítmica: si un marcador de 10 cm se detecta confiablemente a 2 metros de distancia, un marcador de 5 cm solo se detectaría a aproximadamente 1 metro, y un marcador de 20 cm a aproximadamente 4 metros. Esta relación debe considerarse en diseño de espacios de retransmisión: si la distancia entre cámara y marcador es fija, el tamaño del marcador debe seleccionarse apropiadamente para garantizar detección confiable.

## 4. Archivo de Calibración de Cámara: Parametrización de Óptica Específica del Dispositivo

### 4.1 Rol de Calibración en Precisión Geométrica y Corrección de Distorsión

El archivo de calibración de cámara especifica los parámetros intrínsecos y coeficientes de distorsión de la cámara específica utilizada para captura de vídeo. Como se detalló en el apartado anterior (Sección 3), estos parámetros son esenciales para conversión correcta entre coordenadas de píxel en imagen 2D y rayos 3D en espacio del mundo.

La ruta de archivo se especifica como una propiedad del plugin, permitiendo selección de diferentes archivos de calibración sin modificación de código. Esto es crítico en producciones donde múltiples cámaras podrían utilizarse: cada cámara tiene características ópticas únicas (distancia focal, punto principal, patrones de distorsión) que deben medirse mediante calibración específica. Sin calibración apropiada para la cámara actual, la estimación de pose será incorrecta, resultando en desalineamiento sistemático entre contenido virtual y características físicas.

### 4.2 Formato de Archivo y Validación de Parámetros

Los archivos de calibración típicamente se almacenan en formato YAML, con estructura:

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
image_width: 1920
image_height: 1080
```

El módulo aruco_detector debe validar que el archivo especificado existe, que es un archivo YAML válido, y que contiene los campos requeridos. La validación también debe verificar que los parámetros tienen valores razonables: distancias focales fx, fy deben ser positivas y típicamente en rango 500-3000 píxeles para cámaras estándar; punto principal cx, cy debe estar dentro de las dimensiones de imagen; coeficientes de distorsión debe ser en rangos físicamente realizables.

Si el archivo de calibración no se encuentra o es inválido, el sistema debe proporcionar mecanismo de fallback: uso de parámetros de calibración por defecto (asumiendo cámara ideal sin distorsión), o uso de último archivo de calibración válido cargado exitosamente. Esta estrategia de fallback garantiza que retransmisiones no fallen completamente si hay errores de configuración.

### 4.3 Impacto de Calibración Incorrecta y Estrategias de Mitigación

Una calibración incorrecta resulta en errores sistemáticos en estimación de pose que pueden ser difíciles de diagnosticar. Por ejemplo, una calibración de cámara de resolución incorrecta (parámetros calibrados para resolución 1920x1080 pero aplicados a vídeo de resolución 3840x2160) resultaría en distorsión de escala donde el tamaño percibido de marcadores es incorrecto.

Estrategias de mitigación incluyen: (1) validación periódica de calibración mediante comparación de profundidad estimada con profundidad medida manualmente para marcadores conocidos, (2) implementación de auto-calibración donde el sistema detecta desviaciones de pose esperadas y ajusta parámetros de calibración incrementalmente, (3) provisión de herramientas de visualización que muestren parámetros de calibración actual y su impacto visual.

## 5. Offsets de Posición y Rotación: Correcciones Visuales y Adaptación a Desalineamientos

### 5.1 Propósito y Función de Offsets Correctivos

Los offsets de posición y rotación actúan como parámetros de corrección fina que permiten ajustar la ubicación y orientación del contenido virtual respecto a la pose base estimada del marcador detectado. Matemáticamente, los offsets se aplican como transformaciones adicionales compuestas con la transformación base derivada de la detección ArUco:

$$T_{final} = T_{offset} \circ T_{detected}$$

donde Tdetected es la transformación base estimada por el detector ArUco, Toffset es la transformación especificada por los offsets de usuario, y Tfinal es la transformación final utilizada para renderizado de contenido virtual.

Los offsets son particularmente útiles en escenarios donde existe desalineamiento sistemático entre la calibración teórica de la cámara y la realidad física: errores de calibración de cámara sistemáticos, marcadores no perfectamente planos o torcidos, o simplemente requisitos estéticos donde se desea desplazar el contenido virtual ligeramente de su posición de alineamiento perfecto por razones artísticas.

### 5.2 Especificación de Offsets de Posición (X, Y, Z)

Los offsets de posición se especifican como tres parámetros numéricos independientes correspondientes a desplazamientos en direcciones X, Y, Z en unidades de espacio 3D. Un offset de posición positivo en X desplaza el contenido virtual hacia la derecha (desde perspectiva de la cámara), desplazamiento positivo en Y lo mueve hacia arriba, y desplazamiento positivo en Z lo acerca a la cámara.

El rango de valores típicamente es [-1000, 1000] milímetros, permitiendo correcciones de hasta ±1 metro. Los valores deben seleccionarse cuidadosamente basándose en requisitos específicos de producción: offsets demasiado grandes resultarían en desalineamiento visible, mientras que offsets pequeños (centímetros o menos) son típicamente imperceptibles visualmente.

La aplicación de offsets de posición se realiza mediante traslación vectorial:

$$\mathbf{p}_{final} = \mathbf{p}_{detected} + \mathbf{offset}$$

donde pdetected es la posición detectada del marcador en metros, offset es el vector de offset especificado (también en metros), y pfinal es la posición final utilizada para renderizado.

### 5.3 Especificación de Offsets de Rotación (Pitch, Yaw, Roll)

Los offsets de rotación se especifican como tres ángulos en grados, correspondiendo a rotaciones alrededor de ejes X (pitch), Y (yaw), y Z (roll) respectivamente. Estos offsets actúan como rotaciones adicionales aplicadas en orden específico después de la rotación base detectada.

Matemáticamente, se construye una matriz de rotación Roffset mediante composición de rotaciones individuales:

$$R_{offset} = R_z(\text{roll}) \cdot R_y(\text{yaw}) \cdot R_x(\text{pitch})$$

la cual se compone con la matriz de rotación base:

$$R_{final} = R_{offset} \cdot R_{detected}$$

Los offsets de rotación son útiles para corregir errores de estimación de pose sistemáticos donde el marcador, aunque detectado correctamente en posición, se estima como ligeramente rotado comparado con su alineamiento físico real. Pequeños offsets (grados o fracciones de grado) permiten correcciones visuales que mejoran la credibilidad del alineamiento sin ser visualmente perceptibles como artificiales.

### 5.4 Persistencia y Configurabilidad de Offsets

Los valores de offsets se persisten en archivos de configuración del plugin, permitiendo que configuraciones particulares sean guardadas y reutilizadas en futuras retransmisiones. Esta persistencia es importante para consistencia visual en eventos que se repiten: si un offset particular fue efectivo en una retransmisión anterior, puede reutilizarse sin necesidad de recalibración manual.

El sistema puede también soportar perfiles de offset preconfigurados: conjuntos nombrados de valores de offset que se han documentado como funcionando bien para configuraciones específicas de espacio o cámara. Operadores pueden seleccionar un perfil por nombre, en lugar de necesitar especificar valores numéricos individuales, mejorando usabilidad y reduciendo probabilidad de errores de configuración.

### 5.5 Selección de Malla para Modo Reloj

La propiedad de selección de malla para modo reloj especifica qué malla 3D debe renderizarse cuando el plugin opera en modo de visualización de reloj de cuenta atrás, alternativo al modo AR de alineamiento de contenido con marcadores. En contextos de retransmisiones de concursos de programación, es común requerir visualización prominente de contador de tiempo; esta propiedad permite seleccionar la representación geométrica 3D del reloj.

La malla especificada debe existir en los modelos 3D cargados y debe estar totalmente definida (con texturas y materiales). El sistema puede mantener un conjunto de mallas predefinidas de reloj (diseños estándar de contador digital, analógico, o de otro estilo), permitiendo que operadores seleccionen según su preferencia estética. La selección es efectiva únicamente cuando el plugin se configura para operar en modo reloj; en modo AR, esta propiedad no tiene efecto visual.

---

## Conclusión

El conjunto de propiedades de modo AR implementadas constituyen un sistema flexible y exhaustivo que habilita la personalización completa del comportamiento de detección y renderizado de realidad aumentada. La selección de diccionario ArUco determina la capacidad fundamental de detección y robustez; el identificador de marcador permite especificidad de objetivo en escenarios con múltiples marcadores; el tamaño del marcador ancla la estimación de pose a realidad física medible; el archivo de calibración de cámara parametriza las características ópticas específicas del dispositivo de captura; y los offsets de posición y rotación proporcionan mecanismo de corrección fina para adaptación a desalineamientos reales o requisitos estéticos. La interdependencia de estas propiedades requiere cuidado en configuración: seleccionar parámetros inconsistentes (por ejemplo, tamaño de marcador incorrecto, calibración de cámara desactualizada, offsets excesivos) resultaría en desalineamiento sistemático que degradaría experiencia de usuario. Sin embargo, cuando se configuran correctamente, estas propiedades permiten que retransmisiones de concursos de programación logren alineamiento visual preciso entre contenido virtual y características físicas del espacio de retransmisión, contribuyendo significativamente a la credibilidad e impacto visual de la experiencia para audiencias remotas.
