// Inclusión de la biblioteca Assimp (Open Asset Import Library) para la carga de modelos 3D.
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

// Inclusión de las cabeceras del API de OBS Studio.
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <graphics/vec4.h>

// Inclusión de bibliotecas estándar de C.
#include <string.h>
#include <assimp/types.h>
#include "SJ_3DModel.h"


#include <float.h>
float center_x = 0, center_y = 0;

static void free_single_mesh(Mesh *mesh, gs_texture_t *user_texture_to_exclude)
{
	if (!mesh)
		return;
	obs_enter_graphics();
	if (mesh->vb) {
		gs_vertexbuffer_destroy(mesh->vb);
		mesh->vb = NULL;
	}
	if (mesh->ib) {
		gs_indexbuffer_destroy(mesh->ib);
		mesh->ib = NULL;
	}
	
	 // Solo destruye la textura de la malla si NO es la textura que el usuario ha seleccionado.
	// Si mesh->texture es igual a user_texture_to_exclude, significa que es la textura compartida,
	// y esa debe ser destruida solo una vez por el filtro (en cube_filter_destroy).
	if (mesh->texture && mesh->texture != user_texture_to_exclude) {
		gs_texture_destroy(mesh->texture);
		mesh->texture = NULL;
	} else if (mesh->texture == user_texture_to_exclude) {
		// Si la textura de la malla es la textura del usuario,
		// simplemente anula el puntero para que esta malla no intente destruirla.
		// La instancia real de la textura ser� destruida por el filtro m�s tarde.
		mesh->texture = NULL;
	}
	// Si mesh->texture es NULL, no hacemos nada.

	obs_leave_graphics();
}
// --- Función ÚNICA para borrar g_meshes y g_mesh_count ---
void cleanup_global_meshes(struct Mesh **g_meshes, size_t *g_mesh_count,float **mesh_widths, float **mesh_heights, gs_texture_t *user_texture_to_exclude)
{
	if (!*g_meshes)
		return;

	blog(LOG_INFO, "Liberando %zu mallas globales.", *g_mesh_count);

	for (size_t i = 0; i < *g_mesh_count; ++i) {
		free_single_mesh(&(*g_meshes)[i], user_texture_to_exclude);
	}

	bfree(*g_meshes);
	*g_meshes = NULL;
	*g_mesh_count = 0;

	if (mesh_widths && *mesh_widths) {
		bfree(*mesh_widths);
		*mesh_widths = NULL;
	}

	if (mesh_heights && *mesh_heights) {
		bfree(*mesh_heights);
		*mesh_heights = NULL;
	}

	blog(LOG_INFO,"Mallas y arrays de dimensiones liberados correctamente.");
}


/**
 * @brief Carga un modelo 3D desde un archivo utilizando Assimp.
 *
 * Procesa el archivo, extrae la información de las mallas (vértices, índices, texturas)
 * y crea los recursos gráficos correspondientes en OBS.
 *
 * @param path Ruta al archivo del modelo 3D.
 * @return `true` si el modelo se cargó correctamente, `false` en caso contrario.
 */
bool load_model_c(const char *path, Mesh **g_meshes, size_t *g_mesh_count,float **mesh_widths, float **mesh_heights)
{
	// Importa el archivo del modelo usando Assimp.
	// `aiProcess_Triangulate`: Convierte todas las primitivas a triángulos.
	// `aiProcess_FlipUVs`: Invierte las coordenadas de textura en el eje Y.
	// `aiProcess_CalcTangentSpace`: Calcula las tangentes y bitangentes si no existen.
	const struct aiScene *scene =aiImportFile(path, aiProcess_Triangulate | aiProcess_FlipUVs |aiProcess_CalcTangentSpace);

	// Verifica si la importación falló.
	if (!scene) {
		blog(LOG_ERROR, "Assimp error: %s", aiGetErrorString());
		return false;
	}

	// Si ya hay un modelo cargado (g_meshes no es nulo), se liberan sus recursos.
	if (*g_meshes) {
		cleanup_global_meshes(g_meshes, g_mesh_count, mesh_widths,mesh_heights, NULL);
	}

	// Asigna memoria para el nuevo conjunto de mallas del modelo.
	*g_mesh_count = scene->mNumMeshes;
	*g_meshes = (Mesh *)bmalloc(sizeof(Mesh) * (*g_mesh_count));
	*mesh_widths = (float *)bmalloc(sizeof(float) * (*g_mesh_count));
	*mesh_heights = (float *)bmalloc(sizeof(float) * (*g_mesh_count));

	// Itera sobre cada malla en la escena de Assimp.
	for (size_t m = 0; m < *g_mesh_count; m++) {
		struct aiMesh *mesh = scene->mMeshes[m];
		size_t vert_count = mesh->mNumVertices;
		size_t idx_count = mesh->mNumFaces *3; // Cada cara es un triángulo (3 índices).
		
		// Estructura para almacenar los datos de los vértices antes de crear el buffer de OBS.
		struct gs_vb_data *vbd =(struct gs_vb_data *)bmalloc(sizeof(struct gs_vb_data));

		// Asigna memoria para los datos de los vértices.
		vbd->num = vert_count;
		vbd->points = (struct vec3 *)bmalloc(sizeof(struct vec3) *vert_count);
		vbd->normals = (struct vec3 *)bmalloc(sizeof(struct vec3) *vert_count);
		vbd->tangents = (struct vec3 *)bmalloc(sizeof(struct vec3) * vert_count);
		vbd->colors =(uint32_t *)bmalloc(sizeof(uint32_t) * vert_count);
		// --- INICIO: CÁLCULO DE ANCHO Y ALTO ---
		// NUEVO: Inicializar variables para calcular dimensiones aquí
		float min_y = FLT_MAX;
		float max_y = -FLT_MAX;
		float min_x = FLT_MAX;
		float max_x = -FLT_MAX;
		// --- FIN: CÁLCULO DE ANCHO Y ALTO ---
		// Copia los datos de cada vértice desde la estructura de Assimp a nuestra estructura.
		for (size_t i = 0; i < vert_count; i++) {
			// Posiciones
			vbd->points[i].x = mesh->mVertices[i].x;
			vbd->points[i].y = mesh->mVertices[i].y;
			vbd->points[i].z = mesh->mVertices[i].z;
			if (vbd->points[i].y < min_y)
				min_y = vbd->points[i].y;
			if (vbd->points[i].y > max_y)
				max_y = vbd->points[i].y;
			if (vbd->points[i].x < min_x)
				min_x = vbd->points[i].x;
			if (vbd->points[i].x > max_x)
				max_x = vbd->points[i].x;
			// Normales (si existen)
			if (mesh->mNormals) {
				vbd->normals[i].x = mesh->mNormals[i].x;
				vbd->normals[i].y = mesh->mNormals[i].y;
				vbd->normals[i].z = mesh->mNormals[i].z;
			} else {
				vbd->normals[i].x = 0.0f;
				vbd->normals[i].y = 0.0f;
				vbd->normals[i].z = 0.0f;
			}

			// Tangentes (si existen)
			if (mesh->mTangents) {
				vbd->tangents[i].x = mesh->mTangents[i].x;
				vbd->tangents[i].y = mesh->mTangents[i].y;
				vbd->tangents[i].z = mesh->mTangents[i].z;
			} else {
				vbd->tangents[i].x = 0.0f;
				vbd->tangents[i].y = 0.0f;
				vbd->tangents[i].z = 0.0f;
			}

			// Colores de vértice (si existen)
			if (mesh->mColors[0]) {
				uint8_t r = (uint8_t)(mesh->mColors[0][i].r *255.0f);
				uint8_t g = (uint8_t)(mesh->mColors[0][i].g * 255.0f);
				uint8_t b = (uint8_t)(mesh->mColors[0][i].b * 255.0f);
				uint8_t a = (uint8_t)(mesh->mColors[0][i].a * 255.0f);
				// Empaqueta los componentes RGBA en un entero de 32 bits (formato ARGB).
				vbd->colors[i] = (a << 24) | (r << 16) |(g << 8) | b;
			} else {
				vbd->colors[i] =0xFFFFFFFF; // Esto sigue siendo blanco opaco en ARGB
			}
		}

		// Procesa las coordenadas de textura (UVs) si existen.
		if (mesh->mTextureCoords[0]) {
			blog(LOG_WARNING, " tiene coordenadas de textura");
			vbd->num_tex = 1;
			vbd->tvarray = (struct gs_tvertarray *)bmalloc(sizeof(struct gs_tvertarray));
			vbd->tvarray[0].width = 2; // Coordenadas 2D (U, V).
			vbd->tvarray[0].array =bmalloc(sizeof(float) * 2 * vert_count);

			const struct aiVector3D *uvs = mesh->mTextureCoords[0];
			float *uv_array = (float *)vbd->tvarray[0].array;

			// Copia las coordenadas UV.
			for (size_t i = 0; i < vert_count; i++) {
				uv_array[i * 2 + 0] = uvs[i].x;
				uv_array[i * 2 + 1] = uvs[i].y;
			}
		} else {
			blog(LOG_WARNING, " NO tiene coordenadas de textura");
			vbd->num_tex = 0;
			vbd->tvarray = NULL;
		}

		// Asigna memoria y copia los índices de las caras.
		uint32_t *indices =(uint32_t *)bmalloc(sizeof(uint32_t) * idx_count);
		size_t idx_written = 0;
		for (size_t f = 0; f < mesh->mNumFaces; f++) {
			const struct aiFace *face = &mesh->mFaces[f];
			// Asegura que la cara es un triángulo.
			if (face->mNumIndices != 3)
				continue;
			// Copia los tres índices del triángulo.
			indices[idx_written++] = face->mIndices[0];
			indices[idx_written++] = face->mIndices[1];
			indices[idx_written++] = face->mIndices[2];
		}

		// Crea los buffers de la GPU dentro del contexto gráfico de OBS.
		obs_enter_graphics();

		gs_render_start(true);

		// Crea el buffer de vértices y el buffer de índices.
		gs_vertbuffer_t *vb =gs_vertexbuffer_create(vbd, GS_DUP_BUFFER);
		gs_indexbuffer_t *ib = gs_indexbuffer_create(GS_UNSIGNED_LONG, indices, (uint32_t)idx_written,GS_DUP_BUFFER);

		obs_leave_graphics();

		// Almacena los punteros a los buffers y la información de la malla.
		(*g_meshes)[m].vb = vb;
		(*g_meshes)[m].ib = ib;
		(*g_meshes)[m].num_indices = (uint32_t)idx_written;
		(*g_meshes)[m].num_vertex = (uint32_t)vert_count;
		(*g_meshes)[m].texture = NULL; // Initialize to NULL
		(*mesh_heights)[m] = max_y - min_y;
		(*mesh_widths)[m] = max_x - min_x;
		center_x = (max_x + min_x) * 0.5f;
		center_y = (max_y + min_y) * 0.5f;
		blog(LOG_WARNING, "MESH %zu HEIGHT: %f, WIDTH: %f", m,(*mesh_heights)[m], (*mesh_widths)[m]);
		// Libera la memoria temporal usada para los datos de vértices e índices.
		bfree(indices);
		bfree(vbd->points);
		bfree(vbd->normals);
		bfree(vbd->tangents);
		bfree(vbd->colors);

		if (vbd->tvarray) {
			bfree(vbd->tvarray[0].array);
			bfree(vbd->tvarray);
		}
		bfree(vbd);

		// Procesa el material y la textura de la malla.
		struct aiMaterial *material =scene->mMaterials[mesh->mMaterialIndex];
		// Comprueba si el material tiene una textura de tipo difuso.
		if (material && aiGetMaterialTextureCount(
					material, aiTextureType_DIFFUSE) > 0) {
			struct aiString texPath;
			// Obtiene la ruta de la primera textura difusa.
			if (aiGetMaterialTexture(
				    material, aiTextureType_DIFFUSE, 0,
				    &texPath, NULL, NULL, NULL, NULL, NULL,
				    NULL) == AI_SUCCESS) {
				char fullTexPath[512] = {0};
				// Intenta construir la ruta completa de la textura, asumiendo que es relativa al archivo del modelo.

				const char *modelDir = strrchr(path, '/');

				if (!modelDir)
					modelDir = strrchr(path, '\\');

				// Si se encontró un separador de directorio (significa que el modelo está en un subdirectorio).
				if (modelDir) {
					// Calcula la longitud de la ruta del directorio del modelo, incluyendo el separador.
					size_t len = modelDir - path + 1;
					// Copia la parte de la ruta del directorio del modelo a 'fullTexPath'.
					memcpy(fullTexPath, path, len);
					// Asegura que la cadena esté terminada en nulo.
					fullTexPath[len] = '\0';
					// Concatena el nombre de la textura a la ruta base del modelo.
					strncat(fullTexPath, texPath.data,sizeof(fullTexPath) - len - 1);
				} else {
					// Si no se encontró un separador (el modelo está en el "directorio raíz" de la búsqueda).
					// Simplemente copia el nombre de la textura directamente, asumiendo que está en el mismo directorio.
					strncpy(fullTexPath, texPath.data,sizeof(fullTexPath) - 1);fullTexPath[sizeof(fullTexPath) - 1] ='\0';
				}
				// Cargar la imagen
				obs_enter_graphics();
				gs_image_file_t *image =(gs_image_file_t *)bmalloc(sizeof(gs_image_file_t));
				gs_image_file_init(image, fullTexPath);

				gs_image_file_init_texture(image);
				obs_leave_graphics();

				if (image->loaded) {
					blog(LOG_INFO, "Textura cargada: %s",fullTexPath);
					(*g_meshes)[m].texture =image->texture; // Guarda la textura para renderizar.
					gs_image_file_free(image); // Free internal image data
					bfree(image); // Free the image struct
				} else {
					if (image->texture) {
						gs_texture_destroy(image->texture);
						image->texture = NULL;
					}
					blog(LOG_WARNING,"No se pudo cargar textura: %s", fullTexPath);
					gs_image_file_free(image); // Free internal image data
					bfree(image); // Free the image struct
				}
			}
		} else {
			blog(LOG_WARNING, "No tiene materiales");
		}
	}

	obs_leave_graphics();
	// Libera la escena de Assimp para evitar fugas de memoria.
	aiReleaseImport(scene);
	blog(LOG_INFO, "Modelo cargado con %zu mallas", *g_mesh_count);
	return true;
}
/**
 * @brief Aplica una textura dada a todas las mallas del modelo 3D.
 *
 * Esta funci�n itera a trav�s de todas las mallas en el array 'g_meshes'
 * y asigna 'new_texture' a la propiedad 'texture' de cada malla.
 * Es crucial que 'new_texture' sea gestionada externamente (creada y destruida)
 * y que 'free_single_mesh' est� adaptada para no destruir esta textura compartida
 * si se asigna a m�ltiples mallas.
 *
 * @param g_meshes Puntero al array de mallas.
 * @param g_mesh_count N�mero de mallas en el array.
 * @param new_texture La textura (gs_texture_t*) que se asignar� a todas las mallas.
 * Puede ser NULL para "desaplicar" una textura y volver al estado sin textura.
 */
void apply_texture_to_all_meshes(Mesh *g_meshes, size_t g_mesh_count,
				 gs_texture_t *new_texture)
{
	if (!g_meshes) {
		blog(LOG_WARNING, "No hay mallas para aplicar la textura.");
		return;
	}

	for (size_t i = 0; i < g_mesh_count; ++i) {
		// Importante: No destruimos la textura anterior de la malla aqu�.
		// La gesti�n de la memoria de las texturas (ya sean las de Assimp o la 'new_texture')
		// debe ser manejada por las funciones de limpieza (free_single_mesh y cleanup_global_meshes)
		// para evitar "double frees" o fugas de memoria, especialmente si 'new_texture' es compartida.
		g_meshes[i].texture =
			new_texture; // Asigna directamente la nueva textura a la malla
	}

	if (new_texture) {
		blog(LOG_INFO, "Textura '%p' aplicada a todas las %zu mallas.",
		     (void *)new_texture, g_mesh_count);
	} else {
		blog(LOG_INFO, "Textura eliminada de todas las %zu mallas.",
		     g_mesh_count);
	}
}
void render_model_c_NoTexture(Mesh *g_meshes, size_t g_mesh_count, float *widths, float *heights, float scale,float pitch_deg, float yaw_deg, float roll_deg)
{
	// 1) Efecto sólido de OBS
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!solid)
		return;
	gs_eparam_t *col = gs_effect_get_param_by_name(solid, "color");
	if (!col)
		return;
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	if (!tech)
		return;

	// Convertir ángulos a radianes
	float pitch = pitch_deg * (float)M_PI / 180.0f; // X
	float yaw = yaw_deg * (float)M_PI / 180.0f;     // Y
	float roll = roll_deg * (float)M_PI / 180.0f;   // Z

	// Color sólido (rojo)
	struct vec4 c = {1.0f, 0.0f, 0.0f, 1.0f};

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_effect_set_vec4(col, &c);

	for (size_t i = 0; i < g_mesh_count; i++) {
		Mesh *m = &g_meshes[i];
		if (!m->vb || !m->ib)
			continue;

		// Centro local de la malla
		float cx = widths[i] * 0.5f;
		float cy = heights[i] * 0.5f;

		
		// Convertir ángulos a radianes
		float rx = (float)M_PI * pitch_deg / 180.0f;
		float ry = (float)M_PI * yaw_deg / 180.0f;
		float rz = (float)M_PI * roll_deg / 180.0f;
		

		gs_matrix_push();
		// **No** hacemos identity() aquí, así respetamos la traslación global

		//// 1) Mover pivote al origen local (coordenadas NEGATIVAS)
		//	gs_matrix_translate3f(cx, cy, 0.0f);
		gs_matrix_translate3f(-cx, -cy, 0.0f);
		// 2) Aplicar rotaciones en el orden que desees
		//    Por ejemplo: Pitch (X), Yaw (Y), Roll (Z)
		//gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, d*M_PI/180); // Pitch
		//gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, d * M_PI / 180); // Yaw
		//gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, d * M_PI / 180); // Roll
		gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, rx);
		gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, ry);
		gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, rz);

		//// 3) Volver a mover el pivote de regreso (coordenadas POSITIVAS)

		// 4) Escalado uniforme
		gs_matrix_scale3f(scale, scale, scale);
		
		// Dibujar
		gs_load_vertexbuffer(m->vb);
		gs_load_indexbuffer(m->ib);
		gs_draw(GS_TRIS, 0, m->num_indices);

		gs_matrix_pop();
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

int d = 0;
/**
 * @brief Renderiza todas las mallas del modelo cargado.
 *
 * Esta función debe ser llamada en cada fotograma dentro del ciclo de renderizado de OBS.
 */
void render_model_c(Mesh *g_meshes, size_t g_mesh_count, float *widths,
		    float *heights, float scale, float rot_x_deg,
		    float rot_y_deg, float rot_z_deg)
{
	gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!default_effect)return;

	gs_eparam_t *image_param =
		gs_effect_get_param_by_name(default_effect, "image");
	if (!image_param)
		return;
	d = d + 30 * 0.1;
	gs_technique_t *tech = gs_effect_get_technique(default_effect, "Draw");
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	// Convertir ángulos a radianes
	float rx = (float)M_PI * rot_x_deg / 180.0f;
	float ry = (float)M_PI * rot_y_deg / 180.0f;
	float rz = (float)M_PI * rot_z_deg / 180.0f;

	for (size_t i = 0; i < g_mesh_count; i++) {
		if (!g_meshes[i].vb || !g_meshes[i].ib)
			continue;

		if (!g_meshes[i].texture) {
			gs_technique_end_pass(tech);
			gs_technique_end(tech);
			render_model_c_NoTexture(g_meshes, g_mesh_count, widths, heights, scale, rot_x_deg,rot_y_deg,rot_z_deg);
			return;
		}

		// Pivote (centro de la malla)
		float cx = widths[i] * 0.5f;
		float cy = heights[i] * 0.5f;

		gs_matrix_push();
		// **No** hacemos identity() aquí, así respetamos la traslación global

	//// 1) Mover pivote al origen local (coordenadas NEGATIVAS)
	//	gs_matrix_translate3f(cx, cy, 0.0f);
		gs_matrix_translate3f(-cx, -cy, 0.0f);
		// 2) Aplicar rotaciones en el orden que desees
		//    Por ejemplo: Pitch (X), Yaw (Y), Roll (Z)
		//gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, d*M_PI/180); // Pitch
		//gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, d * M_PI / 180); // Yaw
		//gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, d * M_PI / 180); // Roll
		gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, rx);
		gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, ry);
		gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, rz);

		//// 3) Volver a mover el pivote de regreso (coordenadas POSITIVAS)
	


		// 4) Escalado uniforme
		gs_matrix_scale3f(scale, scale, scale);

		// Dibujar la malla
		gs_effect_set_texture(image_param, g_meshes[i].texture);
		gs_load_vertexbuffer(g_meshes[i].vb);
		gs_load_indexbuffer(g_meshes[i].ib);
		gs_draw(GS_TRIS, 0, g_meshes[i].num_indices);

		gs_matrix_pop();
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}
