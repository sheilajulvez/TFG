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

static void free_single_mesh(Mesh *mesh)
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
	if (mesh->texture) {
		gs_texture_destroy(mesh->texture);
		mesh->texture = NULL;
	}
	

	obs_leave_graphics();
}
// --- Función ÚNICA para borrar g_meshes y g_mesh_count ---
void cleanup_global_meshes(struct Mesh **g_meshes, size_t *g_mesh_count,float **mesh_widths, float **mesh_heights)
{
	if (!*g_meshes)
		return;

	blog(LOG_INFO, "Liberando %zu mallas globales.", *g_mesh_count);

	for (size_t i = 0; i < *g_mesh_count; ++i) {
		free_single_mesh(&(*g_meshes)[i]);
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
		cleanup_global_meshes(g_meshes, g_mesh_count, mesh_widths,mesh_heights);
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

void render_model_c_NoTexture(Mesh *g_meshes, size_t g_mesh_count,
			      float *widths, float *heights, float scale)
{
	// Obtener el efecto sólido base de OBS.
	gs_effect_t *solid_effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!solid_effect)
		return;

	gs_eparam_t *color_param =
		gs_effect_get_param_by_name(solid_effect, "color");
	if (!color_param)
		return;

	gs_technique_t *tech = gs_effect_get_technique(solid_effect, "Solid");
	if (!tech)
		return;

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	// Color sólido (rojo opaco)
	struct vec4 solid_color = {1.0f, 0.0f, 0.0f, 1.0f};
	gs_effect_set_vec4(color_param, &solid_color);

	// Itera sobre cada malla del modelo.
	for (size_t i = 0; i < g_mesh_count; i++) {
		if (!g_meshes[i].vb || !g_meshes[i].ib)
			continue;

		gs_matrix_push();

		//// Calcular traslación en X y Y para centrar la malla
		//float translate_x = -(widths[i] * 0.5f) * scale;
		//float translate_y = -(heights[i] * 0.5f) * scale;
		//blog(LOG_INFO,
		//     "Malla %zu (sin textura): Centro X = %.3f (ancho = %.3f), Centro Y = %.3f (alto = %.3f), escala = %.3f",
		//     i, center_x, widths[i], center_y, heights[i], scale);



		//
		//gs_matrix_translate3f(-(center_x*scale), -(center_y*scale), 0.0f);
		// Aplicar escala
		gs_matrix_scale3f(scale, scale, scale);
		// Aplicar traslación para centrar
	

		gs_load_vertexbuffer(g_meshes[i].vb);
		gs_load_indexbuffer(g_meshes[i].ib);
		gs_draw(GS_TRIS, 0, g_meshes[i].num_indices);

			//gs_matrix_pop();
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

/**
 * @brief Renderiza todas las mallas del modelo cargado.
 *
 * Esta función debe ser llamada en cada fotograma dentro del ciclo de renderizado de OBS.
 */
void render_model_c(Mesh *g_meshes, size_t g_mesh_count, float *widths,float *heights, float scale)
{
	gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!default_effect) {
		return;
	}

	gs_eparam_t *image_param =
		gs_effect_get_param_by_name(default_effect, "image");
	if (!image_param) {
		return;
	}

	gs_technique_t *tech = gs_effect_get_technique(default_effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	for (size_t i = 0; i < g_mesh_count; i++) {
		if (!g_meshes[i].vb || !g_meshes[i].ib)
			continue;

		// Si no hay textura, sal del renderizado con textura y llama a la función sin texturas
		if (!g_meshes[i].texture) {
			// Finaliza el efecto actual con textura antes de pasar a uno sin textura
			gs_technique_end_pass(tech);
			gs_technique_end(tech);

			blog(LOG_WARNING,
			     "Malla %zu no tiene textura, usando render_model_c_NoTexture",
			     i);
			render_model_c_NoTexture(g_meshes, g_mesh_count, widths, heights,scale);
			return; // Salimos completamente de esta función
		}

		
		gs_matrix_push();

		//// Calcular traslación en X y Y para centrar la malla
		//float translate_x = -(widths[i] * 0.5f)*scale;
		//float translate_y = -(heights[i] * 0.5f)*scale;

		//blog(LOG_INFO,
		//	 "Malla %zu (sin textura): X = %f (ancho = %f), Y = %f (altura = %f), escala = %f",
		//	 i, translate_x, widths[i], translate_y, heights[i], scale);

		//// Aplicar traslación para centrar
		//gs_matrix_translate3f(translate_x, translate_y, 0.0f);

		//// Aplicar escala
		gs_matrix_scale3f(scale, scale, scale);


		// Aplica textura y dibuja
		gs_effect_set_texture(image_param, g_meshes[i].texture);

		gs_load_vertexbuffer(g_meshes[i].vb);
		gs_load_indexbuffer(g_meshes[i].ib);
		gs_draw(GS_TRIS, 0, g_meshes[i].num_indices);

		// Pop matriz para restaurar estado anterior
		gs_matrix_pop();
	}

	// Solo cerramos técnica si no hubo salida prematura por falta de textura
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}


