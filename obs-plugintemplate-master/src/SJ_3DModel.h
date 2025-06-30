// src/SJ_3DModel.h
#pragma once
#include <stdbool.h>

/**	
 * @brief Estructura para representar una malla (mesh) de un modelo 3D.
 *
 * Contiene los buffers de vértices e índices necesarios para el renderizado,
 * así como la información de la textura asociada.
 */
typedef struct {
	gs_vertbuffer_t *vb;
	gs_indexbuffer_t *ib;
	uint32_t num_indices;
	uint32_t num_vertex;

	gs_texture_t *texture;

} Mesh;
// Declara tus funciones y tipos públicos:
bool load_model_c(const char *path, Mesh **g_meshes, size_t *g_mesh_count);
void render_model_c(Mesh *g_meshes, size_t g_mesh_count);
void cleanup_global_meshes(Mesh **g_meshes, size_t *g_mesh_count);