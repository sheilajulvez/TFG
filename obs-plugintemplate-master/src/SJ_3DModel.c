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

	static bool auto_detect_forward_scene(const struct aiScene *scene,
					      float top_percent,
					      float *out_x_deg,
					      float *out_y_deg,
					      float *out_z_deg)
	{
		if (!scene || scene->mNumMeshes == 0 || !out_x_deg ||
		    !out_y_deg || !out_z_deg)
			return false;

		if (top_percent <= 0.0f || top_percent >= 0.5f)
			top_percent = 0.10f;

		// 1) calcular bbox global en Z (y X/Y por si quieres extender)
		float min_x = FLT_MAX, max_x = -FLT_MAX;
		float min_y = FLT_MAX, max_y = -FLT_MAX;
		float min_z = FLT_MAX, max_z = -FLT_MAX;

		for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
			const struct aiMesh *mesh = scene->mMeshes[m];
			for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
				const struct aiVector3D *v =
					&mesh->mVertices[i];
				if (v->x < min_x)
					min_x = v->x;
				if (v->x > max_x)
					max_x = v->x;
				if (v->y < min_y)
					min_y = v->y;
				if (v->y > max_y)
					max_y = v->y;
				if (v->z < min_z)
					min_z = v->z;
				if (v->z > max_z)
					max_z = v->z;
			}
		}

		const float depth_z = max_z - min_z;
		if (depth_z <= 0.0f)
			return false;

		const float z_threshold = max_z - depth_z * top_percent;
		const float center_x = (max_x + min_x) * 0.5f;
		const float center_y = (max_y + min_y) * 0.5f;
		const float center_z = (max_z + min_z) * 0.5f;

		// 2) acumular vectores (normales si existen, fallback centro->vértice)
		double acc_x = 0.0, acc_y = 0.0, acc_z = 0.0;
		size_t count = 0;
		bool any_normals = false;

		for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
			const struct aiMesh *mesh = scene->mMeshes[m];
			bool mesh_has_normals = (mesh->mNormals != NULL);
			if (mesh_has_normals)
				any_normals = true;

			for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
				const struct aiVector3D *p =
					&mesh->mVertices[i];
				if (p->z < z_threshold)
					continue;

				if (mesh_has_normals) {
					const struct aiVector3D *n =
						&mesh->mNormals[i];
					acc_x += n->x;
					acc_y += n->y;
					acc_z += n->z;
				} else {
					acc_x += (p->x - center_x);
					acc_y += (p->y - center_y);
					acc_z += (p->z - center_z);
				}
				++count;
			}
		}

		// fallback si no hay suficientes vértices en top_percent -> probar top 25%
		if (count == 0) {
			const float z_thr2 = max_z - depth_z * 0.25f;
			for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
				const struct aiMesh *mesh = scene->mMeshes[m];
				bool mesh_has_normals =
					(mesh->mNormals != NULL);

				for (unsigned i = 0; i < mesh->mNumVertices;
				     ++i) {
					const struct aiVector3D *p =
						&mesh->mVertices[i];
					if (p->z < z_thr2)
						continue;

					if (mesh_has_normals) {
						const struct aiVector3D *n =
							&mesh->mNormals[i];
						acc_x += n->x;
						acc_y += n->y;
						acc_z += n->z;
					} else {
						acc_x += (p->x - center_x);
						acc_y += (p->y - center_y);
						acc_z += (p->z - center_z);
					}
					++count;
				}
			}
			if (count == 0)
				return false;
		}

		// promedio
		acc_x /= (double)count;
		acc_y /= (double)count;
		acc_z /= (double)count;

		double len =
			sqrt(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);
		if (len < 1e-6)
			return false;

		double vx = acc_x / len, vy = acc_y / len, vz = acc_z / len;

		// calcular yaw/pitch como en tu heurística por malla
		double yaw = atan2(vx, vz); // rad
		double pitch = atan2(vy, sqrt(vx * vx + vz * vz));

		double yaw_deg = yaw * 180.0 / M_PI;
		double pitch_deg = pitch * 180.0 / M_PI;

		// aplicar signos igual que tu heurístico: -pitch, -yaw para llevar v a +Z
		*out_x_deg = (float)(-pitch_deg);
		*out_y_deg = (float)(-yaw_deg);
		*out_z_deg = 0.0f;

		return true;
	}

	static void free_single_mesh(Mesh *mesh, gs_texture_t *user_texture_to_exclude)
	{
		if (!mesh) return;
		blog(LOG_INFO, "[CUBE] free_single_mesh: mesh=%p, texture=%p", mesh, mesh->texture);
		obs_enter_graphics();
		if (mesh->vb) { gs_vertexbuffer_destroy(mesh->vb); mesh->vb = NULL; }
		if (mesh->ib) { gs_indexbuffer_destroy(mesh->ib); mesh->ib = NULL; }
		if (mesh->texture) { blog(LOG_INFO, "[CUBE] free_single_mesh: anulando textura de malla %p", mesh->texture); mesh->texture = NULL; }
		obs_leave_graphics();
	}
	void cleanup_global_meshes(struct Mesh **g_meshes, size_t *g_mesh_count, float **mesh_widths, float **mesh_heights, gs_texture_t *user_texture_to_exclude)
	{
		if (!*g_meshes) return;
		blog(LOG_INFO, "[CUBE] cleanup_global_meshes: liberando %zu mallas", *g_mesh_count);
		for (size_t i = 0; i < *g_mesh_count; ++i) free_single_mesh(&(*g_meshes)[i], user_texture_to_exclude);
		bfree(*g_meshes);
		*g_meshes = NULL;
		*g_mesh_count = 0;
		if (mesh_widths && *mesh_widths) { bfree(*mesh_widths); *mesh_widths = NULL; }
		if (mesh_heights && *mesh_heights) { bfree(*mesh_heights); *mesh_heights = NULL; }
		blog(LOG_INFO, "[CUBE] Todas las mallas y arrays de dimensiones han sido liberados.");
	}

	
	static void draw_debug_axes(float size)
	{
		struct gs_vb_data vbd;
		vbd.num = 6;
		vbd.points =
			(struct vec3 *)bmalloc(sizeof(struct vec3) * vbd.num);
		vbd.colors = (uint32_t *)bmalloc(sizeof(uint32_t) * vbd.num);
		vbd.num_tex = 0;
		vbd.tvarray = NULL;
		vbd.normals = NULL;
		vbd.tangents = NULL;

		// +X
		vbd.points[0].x = 0;
		vbd.points[0].y = 0;
		vbd.points[0].z = 0;
		vbd.points[1].x = size;
		vbd.points[1].y = 0;
		vbd.points[1].z = 0;
		// +Y
		vbd.points[2].x = 0;
		vbd.points[2].y = 0;
		vbd.points[2].z = 0;
		vbd.points[3].x = 0;
		vbd.points[3].y = size;
		vbd.points[3].z = 0;
		// +Z
		vbd.points[4].x = 0;
		vbd.points[4].y = 0;
		vbd.points[4].z = 0;
		vbd.points[5].x = 0;
		vbd.points[5].y = 0;
		vbd.points[5].z = size;

		vbd.colors[0] = vbd.colors[1] = 0xFFFF0000;
		vbd.colors[2] = vbd.colors[3] = 0xFF00FF00;
		vbd.colors[4] = vbd.colors[5] = 0xFF0000FF;

		obs_enter_graphics();
		gs_vertbuffer_t *vb =
			gs_vertexbuffer_create(&vbd, GS_DUP_BUFFER);
		obs_leave_graphics();

		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
		gs_eparam_t *col = gs_effect_get_param_by_name(solid, "color");
		struct vec4 white = {1, 1, 1, 1};

		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		gs_effect_set_vec4(col, &white);

		gs_load_vertexbuffer(vb);
		gs_draw(GS_LINES, 0, 6);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		obs_enter_graphics();
		gs_vertexbuffer_destroy(vb);
		obs_leave_graphics();

		bfree(vbd.points);
		bfree(vbd.colors);
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
		float model_rot_x = 0.0f, model_rot_y = 0.0f, model_rot_z = 0.0f;
		bool model_has_rot = false;

		if (auto_detect_forward_scene(scene, 0.10f, &model_rot_x, &model_rot_y, &model_rot_z)) {
			model_has_rot = true;
			blog(LOG_INFO, "[AUTO-FWD-MODEL] model rot offsets: x=%f y=%f z=%f",
				 model_rot_x, model_rot_y, model_rot_z);
		} else {
			model_has_rot = false;
			blog(LOG_INFO, "[AUTO-FWD-MODEL] no detection");
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
			(*g_meshes)[m].center_x = 0.0f;
			(*g_meshes)[m].center_y = 0.0f;
			(*g_meshes)[m].center_z = 0.0f;
			(*g_meshes)[m].depth_z = 0.0f;
			(*g_meshes)[m].rot_offset_x = 0.0f;
			(*g_meshes)[m].rot_offset_y = 0.0f;
			(*g_meshes)[m].rot_offset_z = 0.0f;
			(*g_meshes)[m].has_rot_offset = false;
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
			float min_z = FLT_MAX; // <-- nuevo
			float max_z = -FLT_MAX;

			float center_x = 0, center_y = 0;
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
				if (vbd->points[i].z < min_z)
					min_z = vbd->points[i].z;
				if (vbd->points[i].z > max_z)
					max_z = vbd->points[i].z;
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
			(*g_meshes)[m].center_z = (max_z + min_z) * 0.5f;
			// guarda el centro real (usando min/max)
			(*g_meshes)[m].depth_z = (max_z - min_z);
			(*g_meshes)[m].center_x = (max_x + min_x) * 0.5f;
			(*g_meshes)[m].center_y = (max_y + min_y) * 0.5f;
			(*g_meshes)[m].rot_offset_x = model_rot_x;
			(*g_meshes)[m].rot_offset_y = model_rot_y;
			(*g_meshes)[m].rot_offset_z = model_rot_z;
			(*g_meshes)[m].has_rot_offset = model_has_rot;

			//auto_detect_forward(&(*g_meshes)[m], vbd->points,(mesh->mNormals ? vbd->normals : NULL), vert_count,min_x, max_x, min_y, max_y, min_z, max_z, 0.10f);
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
			if (material && aiGetMaterialTextureCount(material, aiTextureType_DIFFUSE) > 0) {
				struct aiString texPath;
				// Obtiene la ruta de la primera textura difusa.
				if (aiGetMaterialTexture( material, aiTextureType_DIFFUSE, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
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
			g_meshes[i].texture =new_texture; // Asigna directamente la nueva textura a la malla
		}

		if (new_texture) {
			blog(LOG_INFO, "Textura '%p' aplicada a todas las %zu mallas.",
				 (void *)new_texture, g_mesh_count);
		} else {
			blog(LOG_INFO, "Textura eliminada de todas las %zu mallas.",
				 g_mesh_count);
		}
	}


	static inline float degrees_to_radians(float degrees){
			return degrees * (float)M_PI / 180.0f;
	}


void render_model_c_NoTexture(Mesh *g_meshes, size_t g_mesh_count,float *widths, float *heights, float scale,
	const float rvec[3], bool detected, float offset_rot_x_deg, float offset_rot_y_deg, float offset_rot_z_deg)
	{
		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		if (!solid)
			return;
		gs_eparam_t *col = gs_effect_get_param_by_name(solid, "color");
		if (!col)
			return;
		gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
		if (!tech)
			return;

		// --- CÁLCULO DE EJE-ÁNGULO (rvec) ---
		float angle_rad = 0.0f;
		float ax = 0.0f, ay = 0.0f, az = 1.0f;
		if (detected) {
			angle_rad = sqrt(rvec[0] * rvec[0] + rvec[1] * rvec[1] +
					 rvec[2] * rvec[2]);
			if (angle_rad > 1e-5f) { // Evitar división por cero
				ax = rvec[0] / angle_rad;
				ay = rvec[1] / angle_rad;
				az = rvec[2] / angle_rad;
			} else {
				angle_rad = 0.0f;
			}
		}
	
		struct vec4 c = {1.0f, 0.0f, 0.0f,
				 1.0f}; 
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		gs_effect_set_vec4(col, &c);

		for (size_t i = 0; i < g_mesh_count; i++) {
			Mesh *m = &g_meshes[i];
			if (!m->vb || !m->ib)
				continue;

			// Centro del modelo (pivote)
			float cx = m->center_x;
			float cy = m->center_y;
			float cz = m->center_z;

			gs_matrix_push();

			// 1. Mover pivote al origen
			gs_matrix_translate3f(-cx, -cy, -cz);
			// 2. Escala
			gs_matrix_scale3f(-scale, scale, -scale);
			// 3. Corrección de coordenadas (Y-Abajo de OpenCV a Y-Arriba de OBS)
			gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, (float)M_PI);

			gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f,
					  degrees_to_radians(offset_rot_x_deg));
			gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f,
					  degrees_to_radians(offset_rot_y_deg));
			gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f,
					  degrees_to_radians(offset_rot_z_deg));

			// 5. Aplicar la rotación del tracker (rvec) SEGUNDO
			if (detected) {
				gs_matrix_rotaa4f(ax, ay, az, angle_rad);
			}
			
			// 6. Dibujar
			gs_load_vertexbuffer(m->vb);
			gs_load_indexbuffer(m->ib);
			gs_draw(GS_TRIS, 0, m->num_indices);

			gs_matrix_pop();
		}

		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}

void render_model_c(Mesh *g_meshes, size_t g_mesh_count, float *widths,	float *heights, float scale, const float rvec[3],	bool detected,float offset_rot_x_deg, float offset_rot_y_deg, float offset_rot_z_deg)
{
	gs_effect_t *default_effect =
		obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!default_effect)
		return;
	gs_eparam_t *image_param =
		gs_effect_get_param_by_name(default_effect, "image");
	if (!image_param)
		return;
	gs_technique_t *tech =
		gs_effect_get_technique(default_effect, "Draw");
	if (!tech)
		return;


	float angle_rad = 0.0f;
	float ax = 0.0f, ay = 0.0f, az = 1.0f;
	if (detected) {
		angle_rad = sqrt(rvec[0] * rvec[0] + rvec[1] * rvec[1] + rvec[2] * rvec[2]);
		if (angle_rad > 1e-5f) {
			ax = rvec[0] / angle_rad;
			ay = rvec[1] / angle_rad;
			az = rvec[2] / angle_rad;
		} else {
			angle_rad = 0.0f;
		}
	}
	

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	for (size_t i = 0; i < g_mesh_count; i++) {
		Mesh *m = &g_meshes[i];
		if (!m->vb || !m->ib)
			continue;

		// Si la malla no tiene textura, delegamos en la versión NoTexture
		if (!m->texture) {
			gs_technique_end_pass(tech);
			gs_technique_end(tech);
			
			// Llamada a la versión SIN textura (pasando todos los parámetros)
			render_model_c_NoTexture(g_meshes, g_mesh_count,
						 widths, heights, scale,
						 rvec, detected,
						 offset_rot_x_deg, offset_rot_y_deg, offset_rot_z_deg);
			return; // Salir para no renderizar doble
		}

		// Centro del modelo (pivote)
		float cx = m->center_x;
		float cy = m->center_y;
		float cz = m->center_z;

		gs_matrix_push();

		// 1. mover pivote al origen 3D
		gs_matrix_translate3f(cx, cy, cz);
		// 2. escala
		gs_matrix_scale3f(-scale, scale, -scale);
		// 3. Corrección de coordenadas (Y-Abajo a Y-Arriba)
		gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, (float)M_PI);

	
        gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, degrees_to_radians(offset_rot_x_deg));
        gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, degrees_to_radians(offset_rot_y_deg));
        gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, degrees_to_radians(offset_rot_z_deg));

	
		if (detected) {
			
			gs_matrix_rotaa4f(ax, -ay, -az, angle_rad); 
		}
	

		gs_effect_set_texture(image_param, m->texture);
		gs_load_vertexbuffer(m->vb);
		gs_load_indexbuffer(m->ib);
		gs_draw(GS_TRIS, 0, m->num_indices);

		gs_matrix_pop();
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

void render_model_clock_c(Mesh *g_meshes, size_t g_mesh_count, float *widths,
			  float *heights, float scale, const float rvec[3],
			  bool detected, float offset_rot_x_deg,
			  float offset_rot_y_deg, float offset_rot_z_deg,
			  int clock_mode,
			  int mesh_id_dial, int mesh_id_hour,
			  int mesh_id_minute, int mesh_id_second,
			  int mesh_id_single,
			  const float *clock_hour_deg,
			  const float *clock_minute_deg,
			  const float *clock_second_deg,
			  const float *clock_single_deg)
{
	gs_effect_t *default_effect =
		obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!default_effect)
		return;
	gs_eparam_t *image_param =
		gs_effect_get_param_by_name(default_effect, "image");
	if (!image_param)
		return;
	gs_technique_t *tech =
		gs_effect_get_technique(default_effect, "Draw");
	if (!tech)
		return;

	float angle_rad = 0.0f;
	float ax = 0.0f, ay = 0.0f, az = 1.0f;
	if (detected) {
		angle_rad = sqrt(rvec[0] * rvec[0] + rvec[1] * rvec[1] + rvec[2] * rvec[2]);
		if (angle_rad > 1e-5f) {
			ax = rvec[0] / angle_rad;
			ay = rvec[1] / angle_rad;
			az = rvec[2] / angle_rad;
		} else {
			angle_rad = 0.0f;
		}
	}

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	for (size_t i = 0; i < g_mesh_count; i++) {
		Mesh *m = &g_meshes[i];
		if (!m->vb || !m->ib)
			continue;

		if (!m->texture)
			continue;

		float cx = m->center_x;
		float cy = m->center_y;
		float cz = m->center_z;


		// Detectar tipo de malla
		bool is_hand = false;
		if (clock_mode == 0) {
			if ((int)i == mesh_id_hour || (int)i == mesh_id_minute || (int)i == mesh_id_second)
				is_hand = true;
		} else if (clock_mode == 1) {
			if ((int)i == mesh_id_single)
				is_hand = true;
		}

		float pivot_y = cy;
		//if (is_hand && heights) {
		//	// USAR max_y en lugar de min_y según feedback del usuario (invertir pivote)
		//	pivot_y = cy + (0.5f * heights[i]);
		//}

		gs_matrix_push();
		gs_matrix_translate3f(-cx, -pivot_y, -cz);
		gs_matrix_scale3f(-scale, scale, -scale);
		gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, (float)M_PI);

		// Rotación ArUco (orientación global - escrito antes = aplicado después al vértice)
		if (detected)
			gs_matrix_rotaa4f(ax, ay, -az, angle_rad);

		// Rotaciones globales opcionales (offset del usuario)
		gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f,
				  degrees_to_radians(offset_rot_x_deg));
		gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f,
				  degrees_to_radians(offset_rot_y_deg));
		gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f,
				  degrees_to_radians(offset_rot_z_deg));

		// Determinar si esta malla necesita rotación de manecilla
		float extra_rotation = 0.0f;
		bool apply_rotation = false;

		if (clock_mode == 0) {
			if ((int)i == mesh_id_hour && clock_hour_deg) {
				extra_rotation = *clock_hour_deg;
				apply_rotation = true;
			} else if ((int)i == mesh_id_minute && clock_minute_deg) {
				extra_rotation = *clock_minute_deg;
				apply_rotation = true;
			} else if ((int)i == mesh_id_second && clock_second_deg) {
				extra_rotation = *clock_second_deg;
				apply_rotation = true;
			}
		} else if (clock_mode == 1) {
			if ((int)i == mesh_id_single && clock_single_deg) {
				extra_rotation = *clock_single_deg;
				apply_rotation = true;
			}
		}

		// Rotación de la manecilla (en espacio del modelo, escrito último = aplicado primero al vértice)
		if (apply_rotation) {
			gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f,
					  degrees_to_radians(360.0f - extra_rotation));
		}


		gs_effect_set_texture(image_param, m->texture);
		gs_load_vertexbuffer(m->vb);
		gs_load_indexbuffer(m->ib);
		gs_draw(GS_TRIS, 0, m->num_indices);

		gs_matrix_pop();
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}


void replace_mesh_textures(struct Mesh *meshes, size_t count,
			   gs_texture_t *new_tex, gs_texture_t *old_tex)
{
	// Para cada sub-malla, libera textura antigua y asigna la nueva
	for (size_t i = 0; i < count; i++) {
		struct Mesh *m = &meshes[i];
		if (old_tex && m->texture == old_tex) {
			gs_texture_destroy(m->texture);
			m->texture = NULL;
		}
		if (new_tex) {
			m->texture = new_tex;
		
		}
	}
}