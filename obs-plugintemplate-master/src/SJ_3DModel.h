// src/SJ_3DModel.h
#pragma once
#include <stdbool.h>

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

// Loads a 3D model and prepares its mesh resources.
// Carga un modelo 3D y prepara sus recursos de malla.
bool load_model_c(const char *path, Mesh **g_meshes, size_t *g_mesh_count,
		  float **mesh_widths, float **mesh_heights);

// Renders the 3D model with the current transform settings.
// Renderiza el modelo 3D con la transformacion actual.
void render_model_c(Mesh *g_meshes, size_t g_mesh_count, float *widths,
		    float *heights, float scale, const float rvec[3],
		    bool detected, float offset_rot_x_deg,
		    float offset_rot_y_deg, float offset_rot_z_deg);

// Renders the 3D model in clock mode.
// Renderiza el modelo 3D en modo reloj.
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

// Releases the current mesh resources and related arrays.
// Libera los recursos de malla actuales y los arrays asociados.
void cleanup_global_meshes(struct Mesh **g_meshes, size_t *g_mesh_count,
			   float **mesh_widths, float **mesh_heights,
			   gs_texture_t *new_texture);

// Applies the same texture to all meshes.
// Aplica la misma textura a todas las mallas.
void apply_texture_to_all_meshes(Mesh *g_meshes, size_t g_mesh_count, gs_texture_t *new_texture);

// Replaces mesh textures using the provided texture handles.
// Reemplaza las texturas de las mallas usando los manejadores dados.
void replace_mesh_textures(struct Mesh *meshes, size_t count, gs_texture_t *new_tex,  gs_texture_t *old_tex);
