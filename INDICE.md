3.1. Arquitectura y Entorno de Desarrollo
3.1.1. Arquitectura del plugin: Aquí va íntegro tu actual 3.0.1 (la estructura cube_filter_data, capas Core/AR/Web).

3.1.2. Configuración del entorno de compilación: Aquí va tu actual 3.0.2 (CMake, dependencias, buildspec.json).

3.2. Motor de Renderizado y Gestión 3D
3.2.1. Carga y gestión de modelos 3D: Aquí mueves tu 3.0.3 (SJ_3DModel, mallas, texturas).

3.2.2. Sistema de reloj avanzado: Aquí va tu 3.0.8 (las manecillas, el módulo countdown_clock y la lógica temporal). Es mejor que esté aquí porque es una extensión del renderizado 3D.

3.2.3. Renderizado de texto 3D y overlays: Aquí va tu 3.0.9. Es el complemento visual final del motor.

3.3. Visión Artificial y Realidad Aumentada
3.3.1. Detección ArUco y calibración de cámara: Aquí va tu actual 3.0.4 (OpenCV, diccionarios, vectores de rotación).

3.3.2. Ciclo de ejecución y renderizado: Aquí mueves tu 3.0.6. Es el sitio ideal porque el ciclo de vida del filtro (tick/render) es donde la AR cobra sentido al procesar el frame.

3.4. Integración con el Ecosistema DOMjudge
3.4.1. Arquitectura de conexión y API: Aquí fusionas 3.0.10 y 3.0.11. Explicas cómo te conectas con curl y cómo parseas el JSON.

3.4.2. Representación de equipos y Scoreboard: Aquí va tu 3.0.12 (la lógica de puntos, el mapeo de IDs y la tabla de posiciones).

3.5. Interfaz y Mantenimiento del Plugin
3.5.1. Propiedades del filtro y funcionalidad: Aquí va tu 3.0.5. Es la explicación de la UI (los menús que ve el usuario en OBS).

3.5.2. Liberación de recursos y limpieza: Aquí va tu 3.0.7. Es el cierre técnico de la implementación.