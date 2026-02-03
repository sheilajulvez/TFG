// src/SJ_3DModel.h
#pragma once
#include <stdbool.h>

/**	
 * @brief Estructura para representar una malla (mesh) de un modelo 3D.
 *
 * Contiene los buffers de v�rtices e �ndices necesarios para el renderizado,
 * as� como la informaci�n de la textura asociada.
 */
typedef struct Mesh {
	gs_vertbuffer_t *vb;
	gs_indexbuffer_t *ib;
	uint32_t num_indices;
	uint32_t num_vertex;
	gs_texture_t *texture;
	float center_x;
	float center_y;
	float center_z; 
	float depth_z; 
	float rot_offset_x;  // offset en pitch (X), en grados
	float rot_offset_y;  // offset en yaw   (Y), en grados
	float rot_offset_z;  // offset en roll  (Z), en grados
	bool has_rot_offset; // true si se calcul� auto/manual



	float pivot_x;
	float pivot_y;
	float pivot_z;

} Mesh;
	 // Declara tus funciones y tipos p�blicos:
bool load_model_c(const char *path, Mesh **g_meshes, size_t *g_mesh_count,float **mesh_widths, float **mesh_heights);
void render_model_c(Mesh *g_meshes, size_t g_mesh_count, float *widths,
		    float *heights, float scale, const float rvec[3],
		    bool detected, float offset_rot_x_deg,
		    float offset_rot_y_deg, float offset_rot_z_deg);

/**
 * Renderiza un modelo 3D en modo reloj con IDs de malla configurables.
 * @param clock_mode 0 = tres manecillas, 1 = una manecilla
 * @param mesh_id_dial ID de la malla del dial (no se rota, -1 para ignorar)
 * @param mesh_id_hour ID de la malla de la manecilla de horas
 * @param mesh_id_minute ID de la malla de la manecilla de minutos
 * @param mesh_id_second ID de la malla de la manecilla de segundos
 * @param mesh_id_single ID de la malla para modo de una manecilla
 * @param clock_hour_deg Ángulo de la manecilla de horas (puede ser NULL)
 * @param clock_minute_deg Ángulo de la manecilla de minutos (puede ser NULL)
 * @param clock_second_deg Ángulo de la manecilla de segundos (puede ser NULL)
 * @param clock_single_deg Ángulo de la manecilla única (puede ser NULL)
 */
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
			 const float *clock_single_deg);


void cleanup_global_meshes(struct Mesh **g_meshes, size_t *g_mesh_count,
			   float **mesh_widths, float **mesh_heights,
			   gs_texture_t *new_texture);
void apply_texture_to_all_meshes(Mesh *g_meshes, size_t g_mesh_count, gs_texture_t *new_texture);
void replace_mesh_textures(struct Mesh *meshes, size_t count, gs_texture_t *new_tex,  gs_texture_t *old_tex);