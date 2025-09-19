// src/SJ_3DModel.h
#pragma once
#include <stdbool.h>

/**	
 * @brief Estructura para representar una malla (mesh) de un modelo 3D.
 *
 * Contiene los buffers de vértices e índices necesarios para el renderizado,
 * así como la información de la textura asociada.
 */
typedef struct Mesh {
	gs_vertbuffer_t *vb;
	gs_indexbuffer_t *ib;
	uint32_t num_indices;
	uint32_t num_vertex;
	gs_texture_t *texture;
	float center_x;
	float center_y;

} Mesh;
	 // Declara tus funciones y tipos públicos:
bool load_model_c(const char *path, Mesh **g_meshes, size_t *g_mesh_count,float **mesh_widths, float **mesh_heights);
void render_model_c(Mesh *g_meshes, size_t g_mesh_count, float *widths, float *heights, float scale,float rot_x_deg, float rot_y_deg, float rot_z_deg);
void cleanup_global_meshes(struct Mesh **g_meshes, size_t *g_mesh_count,float **mesh_widths, float **mesh_heights, gs_texture_t *new_texture);
void apply_texture_to_all_meshes(Mesh *g_meshes, size_t g_mesh_count, gs_texture_t *new_texture);
void replace_mesh_textures(struct Mesh *meshes, size_t count, gs_texture_t *new_tex,  gs_texture_t *old_tex);