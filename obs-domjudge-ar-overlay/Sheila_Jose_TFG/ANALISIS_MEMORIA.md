# Análisis de la Memoria TFG — Mayo 2026

> Generado comparando: (A) comentarios del profesor Pedro en el FDF del 3er borrador, y (B) el estado actual de los ficheros .tex.

---

## PARTE 1 — Comentarios del profesor NO incorporados aún

### 🔴 CRÍTICO (lo pedía explícitamente)

| # | Página PDF | Comentario | Estado actual | Fichero a tocar |
|---|-----------|------------|---------------|-----------------|
| 1 | p.21 (Plan de trabajo) | *"Esto tiene que crear la ilusión de ser un plan. No debe estar en pasado… hay que poner fechas. ¡En el plan no se dice cuál SDK se elige!"* | **SIN CORREGIR.** El plan de trabajo sigue en pasado: *"La primera fase se dedicó al estudio…"*, *"La segunda fase tuvo como objetivo…"*. La tabla tiene duraciones (4 semanas) pero sin fechas concretas. | `Capitulos/Introduccion.tex` |
| 2 | p.21 (Introducción) | *"Falta hablar de los concursos. En la FDI se organizan concursos de programación que se retransmiten en directo y surge la idea de clonar el uso de AR."* | **PARCIALMENTE.** Hay un párrafo genérico sobre concursos universitarios pero no menciona la FDI específicamente ni su contexto real como motivación. | `Capitulos/Introduccion.tex` |
| 3 | p.0 (portada/PDF) | *"Metainformación del PDF"* | **SIN CORREGIR.** `TFGTeXiS.tex` tiene `pdfsubject = {Plantilla de Tesis}` y `pdfkeywords` con texto de plantilla. | `TFGTeXiS.tex` |
| 4 | p.21 (Plan de trabajo) | *"Tal y como está escrito podría usarse en las conclusiones como resumen final del TFG"* | Pendiente de verificar si las conclusiones ya recogen eso o si el plan sigue confundiéndose con las conclusiones. | `Capitulos/Introduccion.tex` |

### 🟡 IMPORTANTE

| # | Página PDF | Comentario | Estado actual | Fichero a tocar |
|---|-----------|------------|---------------|-----------------|
| 5 | p.25 | *"indicar que esto son APIs"* (sobre ARKit y ARCore) | ARKit y ARCore se mencionan como si fueran SDKs/ecosistemas. No se dice explícitamente que son APIs de los sistemas operativos. | `Capitulos/EstadoDeLaCuestion.tex` |
| 6 | p.26–29 | *"Esto no es retransmisión en vivo, que es de lo que trata la sección"* (sobre Noticias, eSports, Educación) | La sección "Aplicaciones de la RA" incluye Noticias (Panorama24), eSports (ESTV), Educación como si fueran ejemplos de retransmisión en vivo equivalentes al Super Bowl. El profesor señaló esto 3 veces. Hay que: o bien enmarcar cada ejemplo indicando que es RA en general (no live), o bien eliminar los no-live y dejar solo los que sí son retransmisión en tiempo real. | `Capitulos/EstadoDeLaCuestion.tex` |
| 7 | p.26 | *"Esto queda raro porque parece que los ejemplos son hitos en el uso de RA. ¿Por qué la Super Bowl es un ejemplo destacado? Bastaría ver las noticias para ver RA en funcionamiento hoy"* | La intro de la sección sigue diciendo *"Un ejemplo destacado es el uso de RA durante el Super Bowl LIX"*. Hay que reenmarcarlo como ejemplo representativo, no como hito. | `Capitulos/EstadoDeLaCuestion.tex` |
| 8 | p.26 | *"Hablar en futuro del pasado suena muy mal"* y *"Esto era una previsión de 2020 según la referencia, que a estas alturas ya o se ha cumplido o no"* | La cita de Ericsson (2020) sobre 5G y la de Grand View Research sobre el mercado en 2023 usan prospectiva que ya es pasado. Hay que actualizar los datos o reformular en pasado. | `Capitulos/EstadoDeLaCuestion.tex` |
| 9 | p.24 | *"las palabras en inglés en cursiva"* | Quedan palabras en inglés sin `\emph{}` o `\textit{}`: `streaming`, `plugin`, `marker-based`, `scoreboard`, `pipeline`, etc. Revisión sistemática pendiente. | Todos los capítulos |
| 10 | p.30 | *"revisar todas las referencias a webs para que estén actualizadas"* | No se ha hecho una revisión de URLs. Algunas referencias de 2020–2022 pueden tener URLs muertas. | `Cascaras/bibliografia.tex` / `biblio.bib` |

### 🟢 POSIBLEMENTE YA CORREGIDO (verificar)

| # | Página PDF | Comentario | Estado probable |
|---|-----------|------------|-----------------|
| 11 | p.24 | *"¿por qué en inglés?"* (encabezados tabla) | La tabla `tiposRA` ya tiene encabezados en español. **OK** |
| 12 | p.24 | *"esto son siglas. Decir cuáles son."* (HMDs, HUD, etc.) | HMDs, HUD, SLAM están expandidos en el texto actual. **OK** |
| 13 | p.25 | *"Referencia"* (faltaba cita) | La zona de SDKs tiene citas. Verificar qué texto exacto estaba sin citar. |
| 14 | p.26 | *"Usar los términos en español"* | El texto ya usa "marcador", "tablero", etc. **OK** |
| 15 | p.33 | *"Reducir la fuente… usar toprule"* en tabla | Tabla `comparativa-sdks` ya usa `\small` y `\toprule`. **OK** |
| 16 | p.30 | *"¿Está cerrado el itemize? Párrafos más estrechos"* | Probablemente corregido al reescribir la sección. Verificar compilando. |
| 17 | p.37 | `<arquitectura>` entre ángulos | Texto actual usa `\path{obs-plugins/arquitectura}` pero sin ángulos. **PARCIAL** |
| 18 | p.38 | *"Justificar esto, que no se 've venir'… 'Modelos 3D' no encaja"* | Sección renombrada a "Renderizado de contenido 3D en OBS". **PARCIAL** — la introducción de la sección aún puede ser más clara en la motivación. |

---

## PARTE 2 — Análisis profundo de la memoria

### Estructura actual de capítulos

```
1. Introducción                          (~3 pág)
2. Estado de la cuestión RA              (~20 pág) ← MUY LARGO
3. Descripción del Trabajo RA            (~20 pág)
4. Estado de la cuestión: Concursos      (~12 pág)
5. Descripción del Trabajo Concursos     (~25 pág) ← EL MÁS LARGO
6. Conclusiones y Trabajo Futuro         (~4 pág)
   + Introducción (EN)                   (~3 pág)
   + Conclusiones (EN)                   (~4 pág)
   + ContribucionesPersonales            (~2 pág)
   + Apéndice A (instalación)
   + Apéndice B (desconocido)
   + Bibliografía
```

Estimación total: **~90–100 páginas**. Para un TFG de 2 personas el mínimo son 35 páginas (25+5+5), así que sobra margen, pero hay secciones que inflan el contenido sin aportar valor.

---

### Secciones que SOBRAN o son demasiado largas

#### 2.1 Tipos de realidad aumentada (EstadoDeLaCuestion.tex)
- Describe tres ejes de clasificación con mucho detalle técnico-académico.
- La tabla `tiposRA` mezcla los tres ejes en 4 filas sin separación clara entre ellos → confusa para el lector.
- **Propuesta:** Reducir a 2 páginas máximo. Explicar los ejes brevemente, dejar la tabla como síntesis visual, y solo profundizar en los tipos que corresponden al proyecto (marker-based, video see-through, aditiva).

#### 2.2 Aplicaciones de la RA (EstadoDeLaCuestion.tex)
- 5 subsecciones (Super Bowl, Noticias, Redes Sociales, eSports, Educación) + 5 figuras grandes.
- El profesor comentó 3 veces que algunos ejemplos NO son retransmisión en vivo.
- **Propuesta:** Recortar a 3 ejemplos máximo, todos de retransmisión en vivo (Super Bowl, eSports, quizá Noticias). Eliminar Redes Sociales y Educación, que son RA en general pero no live streaming. Si se quieren mantener, añadir un párrafo introductorio que diga explícitamente que la sección muestra RA en distintos contextos antes de centrarse en retransmisión.

#### 2.3 SDKs (EstadoDeLaCuestion.tex)
- 5 SDKs × estructura (ventajas/limitaciones/resumen) = 5 bloques repetitivos.
- La decisión tecnológica al final tiene poco peso porque el lector ya lleva 3 páginas leyendo SDKs que se descartan.
- **Propuesta:** Unir la descripción de los SDKs descartados (Vuforia, ARToolKit, NVIDIA, DeepAR) en 1 solo párrafo comparativo + tabla. Desarrollar solo OpenCV/ArUco en detalle porque es el que se usa. La sección bajaría de ~5 páginas a ~2.

#### 2.4.1 OBS Studio como usuario (EstadoDeLaCuestion.tex)
- Muy detallado: escenas, fuentes, filtros, transiciones, mezclador de audio con bullets para cada uno.
- El análisis del Super Bowl como ejemplo de escena en OBS está bien pero largo (3 bloques + 2 figuras).
- **Propuesta:** Mantener la explicación de escenas/fuentes/filtros pero reducir bullets a prosa, eliminar transiciones y mezclador de audio (no son relevantes para el proyecto). El ejemplo del Super Bowl puede quedar pero más condensado.

#### 2.4.2 Desarrollo de plugins (EstadoDeLaCuestion.tex)
- 4 secciones sin numeración (Anatomía, Despliegue, Compilación, C vs C++) muy detalladas.
- La lista de 6 tipos de ficheros de un plugin es demasiado genérica.
- **Propuesta:** Condensar Compilación y C vs C++ en 1 párrafo cada uno. La Anatomía puede reducirse a los tipos de ficheros relevantes para este plugin (dll, shader, datos de calibración), no los 6 genéricos.

#### Contribuciones Personales
- Es completamente genérica: "trabajamos juntos en todo".
- **PROBLEMA GRAVE**: La normativa del TFG exige que cada participante indique su contribución personal con **al menos 2 páginas por persona**. El texto actual no distingue qué hizo Sheila y qué hizo Jose.
- El texto dice *"Aunque no hemos probado el plugin en retransmisiones específicas"* — esto contradice el resto de la memoria que sí presenta resultados.
- **Propuesta:** Reescribir completamente. Sheila: sección específica con sus contribuciones. Jose: sección específica con las suyas. Mínimo 2 páginas por persona. Borrar la frase sobre "no hemos probado".

---

### Secciones que FALTAN o están incompletas

#### 1. Plan de trabajo — ERRÓNEO EN FORMA Y CONTENIDO
El plan de trabajo está escrito como si fuera un resumen de lo hecho. El profesor lo dijo claramente: *"debe estar escrito como si se hubiera hecho al principio del TFG"*, con:
- Tiempo futuro o presente (no pasado)
- Sin revelar qué SDK se eligió (el plan es "elegir una", no "elegimos OpenCV")
- Con fechas concretas (p. ej. "Octubre–Noviembre 2024")
- Sin tanto detalle técnico (las conclusiones de cada fase van a las conclusiones, no aquí)

#### 2. Fechas en el Plan de trabajo
La tabla tiene duraciones (4 semanas, 8 semanas) pero sin fechas reales. Añadir columna de fechas o incluirlas en el texto.

#### 3. FDI / concurso concreto en la Introducción
El profesor dijo específicamente que falta mencionar que en la FDI se organizan concursos que se retransmiten en streaming, y que eso es lo que motiva el proyecto. La Motivación actual habla de "concursos universitarios" en general.

#### 4. ARKit / ARCore como APIs
El texto los presenta como si fueran SDKs autónomos. Hay que aclarar que son **APIs del sistema operativo** (iOS y Android respectivamente) que los desarrolladores usan, no SDKs independientes como Vuforia.

#### 5. SLAM citation en biblio.bib
Pendiente de la sesión anterior.

#### 6. Referencias web actualizadas
Hay referencias de 2020–2022 con URLs que pueden haber cambiado. El profesor lo señaló explícitamente.

#### 7. Metadatos del PDF
`TFGTeXiS.tex` sigue con `pdfsubject = {Plantilla de Tesis}`. Hay que actualizar title, subject, keywords y author.

---

### Problemas de coherencia interna

#### A. "Noticias" como ejemplo de retransmisión en vivo
El ejemplo de Franganillo (Panorama 2024) es RA en televisión pero no es retransmisión de un evento en directo — es un informativo con plató virtual. El profesor lo señaló como "no es retransmisión en vivo". Hay que aclararlo o eliminarlo de esa sección.

#### B. Plan de trabajo vs Conclusiones
El Plan de trabajo actual describe en detalle qué se hizo en cada fase. Las Conclusiones describen lo mismo. Hay contenido duplicado. Con el arreglo del Plan de trabajo (en futuro, sin detalles de implementación) esto se resolvería.

#### C. Contribuciones Personales vs Conclusiones
Las Contribuciones dicen "aunque no hemos probado el plugin en retransmisiones específicas" pero las Conclusiones dicen que el sistema es funcional. Contradicción directa.

#### D. Tabla `tiposRA` vs descripción en texto
La tabla tiene 4 filas mezclando los 3 ejes de clasificación definidos en el texto. El lector no puede ver qué fila corresponde a qué eje. El profesor lo señaló: *"habláis de tres ejes pero en la tabla no se mencionan"*.

---

### Valoración global por capítulo

| Capítulo | Estado | Prioridad |
|----------|--------|-----------|
| Introducción | Bueno pero Plan de Trabajo mal en forma | 🔴 Alta |
| Estado RA | Largo, con secciones desequilibradas, 3 notas del profesor sin atender | 🔴 Alta |
| Descripción Trabajo RA | Bien estructurado, pendiente verificar coherencia coordenadas | 🟡 Media |
| Estado Concursos | Bien | 🟢 Baja |
| Descripción Trabajo Concursos | Muy detallado, parece el más sólido técnicamente | 🟢 Baja |
| Conclusiones | Bien escritas, aunque algo cortas en Trabajo Futuro | 🟡 Media |
| ContribucionesPersonales | **Reescribir completamente** | 🔴 Alta |
| Resumen/Abstract | Muy bien | 🟢 OK |
| Apéndice A (instalación) | Bien | 🟢 OK |

---

## PARTE 3 — Plan de Acción

### Bloque 1 — OBLIGATORIO antes de entrega (comentarios directos del profesor)

**B1.1 — Introduccion.tex: Plan de trabajo**
- Cambiar todo a tiempo futuro/condicional ("La primera fase se dedicará a…")
- Añadir fechas reales en la tabla (Octubre–Noviembre 2024, etc.)
- Eliminar referencias a "elegimos OpenCV" o cualquier decisión concreta
- Diferenciar bien de las Conclusiones (el plan es intención, no resultado)

**B1.2 — Introduccion.tex: Mencionar FDI y sus concursos**
- Añadir en Motivación: "En la Facultad de Informática (FDI-UCM) se organizan concursos de programación que se retransmiten en directo…" como ejemplo concreto que motivó el TFG.

**B1.3 — ContribucionesPersonales.tex: Reescritura completa**
- Sección "Contribuciones de Sheila Julvez López" (~2 páginas)
- Sección "Contribuciones de Jose Moreno Barbero" (~2 páginas)
- Específico: qué investigó cada uno, qué implementó, qué redactó
- Eliminar "aunque no hemos probado el plugin…"

**B1.4 — TFGTeXiS.tex: Metadatos del PDF**
- `pdftitle`, `pdfsubject`, `pdfkeywords`, `pdfauthor` actualizados
- Portada: convocatoria "Junio de 2026" sin día específico

### Bloque 2 — IMPORTANTE (comentarios del profesor + análisis propio)

**B2.1 — EstadoDeLaCuestion.tex: Aplicaciones de la RA**
- Añadir intro que enmarque la sección como "RA en diferentes contextos" antes de centrarse en live
- Eliminar o reubicar el ejemplo de Educación
- Reformular el Super Bowl como ejemplo representativo (no "destacado" / "hito")
- Verificar coherencia: ¿Noticias de Panorama es live? → Aclarar o eliminar
- Corregir previsión Ericsson 2020 (ya no es futuro) y datos Grand View Research

**B2.2 — EstadoDeLaCuestion.tex: SDKs section**
- Condensar Vuforia, ARToolKit, NVIDIA, DeepAR en 1 párrafo + tabla
- Profundizar solo en OpenCV + ArUco (que es el elegido)
- Añadir explícitamente que ARKit/ARCore son "APIs del SO"

**B2.3 — EstadoDeLaCuestion.tex: Tabla tiposRA**
- Reestructurar para que los 3 ejes sean visibles en la tabla
- Por ejemplo: añadir columna "Eje" o agrupar filas por eje con `\midrule`

**B2.4 — Revisión sistemática de palabras en inglés**
- En todos los capítulos: poner en `\textit{}` las palabras en inglés no consolidadas
- Lista de palabras a buscar: streaming, plugin, pipeline, scoreboard, frame, overlay, callback, shader, marker, tracking, freeze, reveal

**B2.5 — Verificar y actualizar referencias web**
- Comprobar URLs de las referencias web en biblio.bib
- Añadir `urldate` o `note` con fecha de acceso donde falte

### Bloque 3 — MEJORAS (análisis propio, no comentarios directos)

**B3.1 — Reducir OBS Studio como usuario**
- Condensar bullets de escenas/fuentes/filtros/transiciones/mezclador a prosa más fluida
- Mantener figuras pero unificar algunas

**B3.2 — DescripcionTrabajo.tex: Verificar la sección de transformaciones de coordenadas**
- Había una posible contradicción en la explicación del convenio de ejes (tarea #17 pendiente)

**B3.3 — Añadir entrada SLAM a biblio.bib** (tarea #12 pendiente)

**B3.4 — Introduction.tex (inglés): Mismos cambios que Introduccion.tex**
- Sincronizar los cambios del Plan de trabajo y FDI con la versión inglesa

**B3.5 — ConclusionsFutureWork.tex: Ampliar Trabajo Futuro**
- Las 4 líneas futuras están bien pero son algo escuetas
- Podría añadirse: multiplataforma (Linux/macOS), múltiples marcadores simultáneos, UI de retransmisión remota (ya mencionada)

---

## Resumen ejecutivo: 10 cosas a hacer en orden de prioridad

1. 🔴 **Reescribir Plan de trabajo** — futuro, con fechas, sin decisiones técnicas concretas
2. 🔴 **Reescribir ContribucionesPersonales** — específico por persona, ≥2 pág por persona
3. 🔴 **Añadir párrafo de FDI** en Motivación (Introduccion.tex)
4. 🔴 **Metadatos del PDF** (TFGTeXiS.tex)
5. 🟡 **Reformular sección Aplicaciones RA** — quitar o enmarcar los no-live, reformular Super Bowl
6. 🟡 **Actualizar datos obsoletos** — Ericsson 2020, Grand View Research 2023
7. 🟡 **ARKit/ARCore = APIs** — aclarar en el texto
8. 🟡 **Palabras en inglés en cursiva** — revisión sistemática
9. 🟡 **Tabla tiposRA** — hacer los ejes visibles
10. 🟢 **Condensar SDKs descartados** en tabla + párrafo (opcional si hay espacio)
