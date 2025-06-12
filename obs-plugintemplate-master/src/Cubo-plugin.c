#include <obs-module.h>
#include "graphics/graphics.h"
#include "graphics/effect.h"
#include "graphics/vec4.h"
#include <graphics/image-file.h>
#include "graphics/quat.h"
#include <obs.h>
#include <plugin-support.h>
#include "Windows.h"
#include <util/platform.h>  // bmemdup, bmalloc, etc. (memoria multiplataforma)
#include "obs-config.h"






OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cube", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "PLANO";
}
struct cube_filter_data {
	gs_zstencil_t *zstencil;
	obs_source_t *source;
	gs_texture_t *texture;
	int width, height;
	int pox, posy;
	float rotation_z;

	gs_texrender_t *texrender; //  Usa esto en su lugar
};
int vel = 100;
static uint32_t cube_source_get_width(void *data)
{
	UNUSED_PARAMETER(data);
	return 0; // O el ancho que tú quieras
}

static uint32_t cube_source_get_height(void *data)
{
	UNUSED_PARAMETER(data);
	return 0; // O la altura deseada
}

void image_source_load(gs_image_file_t *image, const char *file)
{
	obs_enter_graphics();

	gs_image_file_free(image);

	obs_leave_graphics();

	gs_image_file_init(image, file);

	obs_enter_graphics();

	gs_image_file_init_texture(image);

	obs_leave_graphics();

	if (!image->loaded) {
		blog(LOG_WARNING, "failed to load texture %s", file);
	}
}

static gs_vertbuffer_t *cube_faces[6];

static gs_indexbuffer_t *indexbuffer; //triangulos 
static gs_vertbuffer_t *vertexbuffer; //vertices

// Vértices del cubo (8 vértices)
struct vec3 cube_vertices[8] = {
	{0, 0, 0},   // 0
	{50, 0, 0},  // 1
	{0, 50, 0},  // 2
	{50, 50, 0}, // 3
	{0, 0, 50},  // 4
	{50, 0, 50}, // 5
	{0, 50, 50}, // 6
	{50, 50, 50} // 7
};
// Índices para las 12 caras del cubo (36 índices)
static const uint16_t cube_indices[] = {
    0, 1, 3,  3, 2, 0,  // Cara trasera  (Z=0)
    4, 6, 7,  7, 5, 4,  // Cara delantera (Z=50)
    0, 2, 6,  6, 4, 0,  // Cara izquierda (X=0)
    1, 5, 7,  7, 3, 1,  // Cara derecha  (X=50)
    2, 3, 7,  7, 6, 2,  // Cara superior (Y=50)
    0, 4, 5,  5, 1, 0   // Cara inferior (Y=0)
};

struct cube_vertex {
	struct vec3 pos;
	uint32_t color;
};

static int size = 50; // Ejemplo, ajustar según necesidad

static void update_vertices(void)
{
	obs_enter_graphics();
	gs_render_start(true);
	// --- LINE VERTICES ---
	////cara 1

	// Cara frontal (z = size)
	size = 50;
	gs_vertex3f(0, 0, 0);
	gs_vertex3f(size, 0, 0);
	gs_vertex3f(0, size, 0);

	gs_vertex3f(0, size, 0);
	gs_vertex3f(size, size, 0);
	gs_vertex3f(size, 0, 0);
	cube_faces[0] = gs_render_save();
	//atras
	gs_vertex3f(0, 0, size);
	gs_vertex3f(size, 0, size);
	gs_vertex3f(0, size, size);

	gs_vertex3f(0, size, size);
	gs_vertex3f(size, size, size);
	gs_vertex3f(size, 0, size);
	cube_faces[1] = gs_render_save();

	// Cara derexha (z = 0) SI

	gs_vertex3f(size, 0, 0);
	gs_vertex3f(size, 0, size);
	gs_vertex3f(size, size, 0);

	gs_vertex3f(size, size, 0);
	gs_vertex3f(size, size, size);
	gs_vertex3f(size, 0, size);
	cube_faces[2] = gs_render_save();

	// Cara izquierda  SI
	gs_vertex3f(0, 0, size);
	gs_vertex3f(0, 0, 0);
	gs_vertex3f(0, size, size);

	gs_vertex3f(0, size, size);
	gs_vertex3f(0, size, 0);
	gs_vertex3f(0, 0, 0);
	cube_faces[3] = gs_render_save();
	// Cara arriba (y = 0)

	gs_vertex3f(0, 0, size);
	gs_vertex3f(size, 0, size);
	gs_vertex3f(0, 0, 0);

	gs_vertex3f(0, 0, 0);
	gs_vertex3f(size, 0, 0);
	gs_vertex3f(size, 0, size);
	cube_faces[4] = gs_render_save();
	// Cara abajo (x = 0)
	gs_vertex3f(0, size, size);
	gs_vertex3f(size, size, size);
	gs_vertex3f(0, size, 0);

	gs_vertex3f(0, size, 0);
	gs_vertex3f(size, size, 0);
	gs_vertex3f(size, size, size);
	cube_faces[5] = gs_render_save();


		
	//line_vert  = gs_render_save();


	obs_leave_graphics();




	// No liberar dot_data ni dot_data->points (la función gs_vertexbuffer_create toma la propiedad)
}
// Función para crear una textura blanca para la pizarra
void create_whiteboard_texture(struct cube_filter_data *data)
{
	obs_enter_graphics();

	if (data->texture != NULL) {
		gs_texture_destroy(data->texture);
		data->texture = NULL;
	}
	data->texrender = gs_texrender_create(GS_RGBA, GS_Z32F);
	if (!data->texrender) {
		blog(LOG_ERROR, "❌ No se pudo crear texrender");
	}
	data->texture = gs_texture_create(data->width, data->height, GS_RGBA, 1,
					  NULL, GS_RENDER_TARGET);
	data->zstencil = gs_zstencil_create(data->width, data->height, GS_Z32F);
	blog(LOG_INFO, "create whiteboard texture %d %d", data->width,
	     data->height);

	obs_leave_graphics();
}

static const char *cube_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused); // Macro común en OBS para evitar warnings
	return "Cubo 3D (Índices y UV, sin textura)";
}
static void *cube_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct cube_filter_data *data =bzalloc(sizeof(struct cube_filter_data));
	data->source = source;
	data->pox = 0;
	data->posy = 0;
	data->rotation_z = 90;
	obs_enter_graphics();
	gs_render_start(true);

	struct gs_vb_data *vbd = gs_vbdata_create();
	vbd->num = 8;
	vbd->points = bmemdup(cube_vertices, sizeof(cube_vertices));
	// Colores distintos por vértice
	struct vec4 cube_colors[8] = {
		{1, 0, 0, 1},    // Rojo
		{0, 1, 0, 1},    // Verde
		{0, 0, 1, 1},    // Azul
		{1, 1, 0, 1},    // Amarillo
		{1, 0, 1, 1},    // Magenta
		{0, 1, 1, 1},
		{1, 0.5f, 0, 1} // Naranja
	};

	vbd->colors = bmemdup(cube_colors, sizeof(cube_colors));
	// Crear el vertex buffer con los 8 vértices únicos
	vertexbuffer = gs_vertexbuffer_create(vbd, 0);
	uint16_t *indices_dup = bmemdup(cube_indices, sizeof(cube_indices));
	indexbuffer =gs_indexbuffer_create(GS_UNSIGNED_SHORT, indices_dup, 36, 0);
	obs_leave_graphics();
	// Obtener la resolución del vídeo de salida
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		data->width = ovi.base_width;
		data->height = ovi.base_height;
		create_whiteboard_texture(data);
	} 
	else {
		blog(LOG_WARNING, "Whiteboard: Failed to get video resolution");
	}

	return data;
}

static void cube_filter_destroy(void *data)
{
	if (vertexbuffer)
		gs_vertexbuffer_destroy(vertexbuffer);
	if (indexbuffer)
		gs_indexbuffer_destroy(indexbuffer);
	/*if (data->texrender)
		gs_texrender_destroy(filter->texrender);*/

	//bfree(filter);
}

static void cube_filter_render(void *data, gs_effect_t *effect1)
{
	struct cube_filter_data *filter = (struct cube_filter_data *)data;

	// Obtener el efecto base por defecto
	gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	obs_enter_graphics();
	if (effect) {
		gs_blend_state_push();
		gs_reset_blend_state();
		gs_matrix_push();
		gs_matrix_identity();

		while (gs_effect_loop(effect, "Draw")) {
			obs_source_draw(gs_texrender_get_texture(filter->texrender), 0, 0, filter->width, filter->height, false);
			obs_source_draw(filter->texture, 0, 0, 0, 0, false);
		}

		gs_matrix_pop();
		gs_blend_state_pop();
	}
	obs_leave_graphics();
}

static void cue_filter_tick(void *data, float seconds)
{
	struct cube_filter_data *filter = data;
	struct obs_video_info video_info;


	// Actualizar dimensiones si han cambiado
	if (obs_get_video_info(&video_info)) {
		if (video_info.base_width != filter->width ||
		    video_info.base_height != filter->height) {
			filter->width = video_info.base_width;
			filter->height = video_info.base_height;
			create_whiteboard_texture(filter);
		}
	}

	obs_enter_graphics();
	gs_viewport_push();
	gs_set_viewport(0, 0, filter->width, filter->height);
	gs_projection_push();
	gs_set_3d_mode(60.0f, 0.1f, 1000.0f);
	gs_blend_state_push();
	gs_reset_blend_state();

	gs_enable_depth_test(true);
	gs_depth_function(GS_LESS);
	//gs_set_cull_mode(GS_BACK);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	const struct vec4 cube_colorsfaces[6] = {
		{1, 0, 0, 1},  // Rojo
		{0, 1, 0, 1},  // Verde
		{0, 0, 1, 1},  // Azul
		{1, 1, 0, 1},  // Amarillo
		{1, 0, 1, 1},  // Magenta
		{0, 1, 1, 1}
	};


	//// --- Cubo 1 TEXTURA
	gs_texture_t *prev_render_target = gs_get_render_target();
	gs_texture_t *prev_zstencil_target = gs_get_zstencil_target();
	gs_set_render_target(filter->texture, filter->zstencil); //TEXTURA

	gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH,
		 (float[]){0.0f, 0.0f, 0.0f, 0.0f}, 1.0f, 0);
	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_translate3f(filter->pox + 500, filter->posy + 300,0.0f); 
	filter->rotation_z += 45.0f * seconds;
	if (filter->rotation_z >= 360.0f) filter->rotation_z -= 360.0f;
	gs_matrix_rotaa4f(1.0f, 1.0f, 1.0f,filter->rotation_z * (float)M_PI / 180.0f);
	gs_matrix_scale3f(1.0f, 1.0f, 1.0f);

	gs_load_vertexbuffer(vertexbuffer);
	gs_load_indexbuffer(indexbuffer);
	for (int i = 0; i < 6; i++) {
		gs_effect_set_vec4(color_param, &cube_colorsfaces[i]);
		gs_draw(GS_TRIS, i * 6, 6);
	}
	gs_matrix_pop();
	gs_set_render_target(prev_render_target, prev_zstencil_target);


	gs_technique_end_pass(tech);
	gs_technique_end(tech);
	gs_projection_pop();
	gs_viewport_pop();
	gs_blend_state_pop();
	obs_leave_graphics();


	
}

static struct obs_source_info cube_filter = {
	.id = "cube_filter",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = cube_filter_get_name,
	.create = cube_filter_create,
	.destroy = cube_filter_destroy,
	.video_render = cube_filter_render,
	.video_tick = cue_filter_tick,
	.get_width = cube_source_get_width,
	.get_height = cube_source_get_height,
	
	
};

bool obs_module_load(void)
{
	blog(LOG_INFO, "[CUBE] Registrando filtro");

	obs_register_source(&cube_filter);
	blog(LOG_INFO, "[CUBE] Registrade");
	return true;
}



