# Plan de imágenes pendientes de insertar en el TFG

Todas las imágenes hay que copiarlas a: `Imagenes/Vectorial/`

---

## 1. `modos_plugin`
**Capítulo:** Descripción del Trabajo Concursos de Programación  
**Sección:** `\section{Modos de visualización}` (subsección 3.5.6, dentro de Conectividad con DOMjudge)  
**Posición:** Al principio de la sección, antes de describir cada modo individualmente.  
**Caption sugerido:**  
> Interfaz del plugin mostrando los distintos modos de funcionamiento disponibles: modo 3D manual, modo AR, modo cuenta atrás, modo marcador/scoreboard y modo información de equipo.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.75\textwidth]{Imagenes/Vectorial/modos_plugin}
  \caption{Modos de funcionamiento del plugin.}
  \label{fig:modos_plugin}
\end{figure}
```

---

## 2. `UI_modo3D`
**Capítulo:** Descripción del Trabajo Realidad Aumentada  
**Sección:** `\subsection{Modelo 3D y textura}` (sección 5.4.3)  
**Posición:** Al inicio de esa subsección, mostrando los controles disponibles.  
**Caption sugerido:**  
> Panel de configuración del modo 3D: carga del modelo, textura, posición, rotación y escala.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.72\textwidth]{Imagenes/Vectorial/UI_modo3D}
  \caption{Configuración del modo 3D en la interfaz del plugin.}
  \label{fig:ui_modo3d}
\end{figure}
```

---

## 3. `UI_modo3DAR`
**Capítulo:** Descripción del Trabajo Realidad Aumentada  
**Sección:** `\subsection{Offsets de posición y rotación}` (sección 5.4.4)  
**Posición:** Tras la explicación de los offsets, como ilustración de los controles específicos de AR.  
**Caption sugerido:**  
> Parámetros adicionales de la interfaz para el modo AR con modelo 3D: offsets de posición y rotación relativos al marcador.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.72\textwidth]{Imagenes/Vectorial/UI_modo3DAR}
  \caption{Parámetros de AR con modelo 3D en la interfaz del plugin.}
  \label{fig:ui_modo3dar}
\end{figure}
```

---

## 4. `UI_conexionweb`
**Capítulo:** Descripción del Trabajo Concursos de Programación  
**Sección:** `\subsection{Configuración del servidor}` (sección 6.4.1)  
**Posición:** Al inicio, como muestra de la interfaz de conexión.  
**Caption sugerido:**  
> Panel de configuración de la conexión web al servidor DOMjudge: URL base, ID del concurso, credenciales e intervalo de actualización.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.72\textwidth]{Imagenes/Vectorial/UI_conexionweb}
  \caption{Configuración de la conexión web al servidor DOMjudge.}
  \label{fig:ui_conexionweb}
\end{figure}
```

---

## 5. `UI_modoTexto1` y `UI_modoTexto2`
**Capítulo:** Descripción del Trabajo Concursos de Programación  
**Sección:** `\subsection{Modos de visualización}` → subsección de texto dinámico (o al inicio de `\section{Renderizado de texto dinámico}`, sección 6.1)  
**Posición:** Mostrar ambas imágenes juntas con `\subfigure` o en páginas consecutivas.  
**Caption sugerido:**  
> Opciones de personalización del texto en el plugin: fuente, tamaño, color y alineación, usables en los modos scoreboard e información de equipo.

**Código LaTeX (subfiguras lado a lado):**
```latex
\begin{figure}[h]
  \centering
  \begin{subfigure}[b]{0.48\textwidth}
    \includegraphics[width=\textwidth]{Imagenes/Vectorial/UI_modoTexto1}
    \caption{Personalización de texto (I).}
    \label{fig:ui_texto1}
  \end{subfigure}
  \hfill
  \begin{subfigure}[b]{0.48\textwidth}
    \includegraphics[width=\textwidth]{Imagenes/Vectorial/UI_modoTexto2}
    \caption{Personalización de texto (II).}
    \label{fig:ui_texto2}
  \end{subfigure}
  \caption{Opciones de personalización del overlay de texto del plugin.}
  \label{fig:ui_texto}
\end{figure}
```
*(Requiere `\usepackage{subcaption}` en el preámbulo)*

---

## 6. `UI_modoTeamInfo`
**Capítulo:** Descripción del Trabajo Concursos de Programación  
**Sección:** `\subsection{Mapeo de marcadores a equipos}` (sección 6.4.3)  
**Posición:** Al inicio, mostrando el control de carga del JSON.  
**Caption sugerido:**  
> Panel exclusivo del modo información de equipo: carga del fichero JSON que asocia identificadores de marcador con equipos participantes.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.72\textwidth]{Imagenes/Vectorial/UI_modoTeamInfo}
  \caption{Configuración del modo información de equipo.}
  \label{fig:ui_teaminfo}
\end{figure}
```

---

## 7. `Ejemplomodo3d`
**Capítulo:** Descripción del Trabajo Realidad Aumentada  
**Sección:** `\section{Renderizado de Objetos 3D y Coherencia Espacial}` (sección 5.4 / similar)  
**Posición:** Al final de la sección, como resultado visual del modo 3D.  
**Caption sugerido:**  
> Ejemplo del modo 3D en funcionamiento: tres instancias del filtro con el mismo modelo de caja en distintas posiciones de la escena de OBS.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.85\textwidth]{Imagenes/Vectorial/Ejemplomodo3d}
  \caption{Modo 3D: tres filtros con el mismo modelo 3D en diferentes posiciones.}
  \label{fig:ejemplo_modo3d}
\end{figure}
```

---

## 8. `ModoScoreboard`
**Capítulo:** Descripción del Trabajo Concursos de Programación  
**Sección:** `\subsection{Modos de visualización}` (sección 6.3.6, al final donde se describe el modo scoreboard)  
**Posición:** Al final de la descripción del modo scoreboard, como captura del resultado.  
**Caption sugerido:**  
> Modo scoreboard en funcionamiento: overlay de texto sobre marcador ArUco con datos de prueba obtenidos de la API REST de DOMjudge.

**Código LaTeX:**
```latex
\begin{figure}[h]
  \centering
  \includegraphics[width=0.85\textwidth]{Imagenes/Vectorial/ModoScoreboard}
  \caption{Modo scoreboard: overlay AR con datos del servidor DOMjudge de prueba.}
  \label{fig:modo_scoreboard}
\end{figure}
```

---

## Notas generales para Overleaf

1. **Subir imágenes:** en Overleaf, menú izquierdo → botón de subida → carpeta `Imagenes/Vectorial/`.
2. **Formato de archivo:** si las imágenes son `.png`, incluirlas tal cual. Si son `.jpg`, también funciona. Para `.pdf` vectorial es mejor.
3. **Paquete subcaption:** añadir en `TeXiS/TeXiS_pream.tex`:  
   ```latex
   \usepackage{subcaption}
   ```
4. **Referencias cruzadas:** usar `\ref{fig:xxx}` en el texto para referenciar cada figura, p. ej.:  
   > *Como se muestra en la figura~\ref{fig:modos_plugin}, el plugin ofrece cinco modos...*
