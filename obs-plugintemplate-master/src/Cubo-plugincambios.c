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
#include "obs-defs.h"
#include "obs-data.h"
#include "obs-properties.h"
#include "obs-interaction.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cube", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "PLANO";
}

struct cube_filter_data {
	obs_source_t *source;
	gs_texture_t *texture;
	int width, height;
	int pox, posy;
	float rotation_z;
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

static gs_vertbuffer_t *vertexbuffers_faces[6] = {NULL};
static gs_indexbuffer_t *indexbuffers_faces[6] = {NULL};

// Vértices por cara (6 vértices por cara, 2 triángulos)
static const struct vec3 face_vertices[6][6] = {
	// Cara trasera (Z=0)
	{{0, 0, 0}, {50, 0, 0}, {50, 50, 0}, {50, 50, 0}, {0, 50, 0}, {0, 0, 0}},
	// Cara delantera (Z=50)
	{{0, 0, 50},
	 {50, 0, 50},
	 {50, 50, 50},
	 {50, 50, 50},
	 {0, 50, 50},
	 {0, 0, 50}},
	// Cara izquierda (X=0)
	{{0, 0, 0}, {0, 0, 50}, {0, 50, 50}, {0, 50, 50}, {0, 50, 0}, {0, 0, 0}},
	// Cara derecha (X=50)
	{{50, 0, 0},
	 {50, 0, 50},
	 {50, 50, 50},
	 {50, 50, 50},
	 {50, 50, 0},
	 {50, 0, 0}},
	// Cara superior (Y=50)
	{{0, 50, 0},
	 {50, 50, 0},
	 {50, 50, 50},
	 {50, 50, 50},
	 {0, 50, 50},
	 {0, 50, 0}},
	// Cara inferior (Y=0)
	{{0, 0, 0}, {50, 0, 0}, {50, 0, 50}, {50, 0, 50}, {0, 0, 50}, {0, 0, 0}},
};

// Índices siempre de 0 a 5 (6 vértices por cara)
static const uint16_t face_indices[6] = {0, 1, 2, 3, 4, 5};

// Colores por cara
static struct vec4 face_colors[6] = {
	{1, 0, 0, 1}, // rojo
	{0, 1, 0, 1}, // verde
	{0, 0, 1, 1}, // azul
	{1, 1, 0, 1}, // amarillo
	{1, 0, 1, 1}, // magenta
	{0, 1, 1, 1}  // cyan
};

static void create_face_buffers(void)
{
	obs_enter_graphics();

	for (int i = 0; i < 6; i++) {
		struct gs_vb_data vbd = {0};
		vbd.num = 6;
		vbd.points = bmemdup(face_vertices[i], sizeof(struct vec3) * 6);

		// Colores para los 6 vértices (igual para cada uno, el color de la cara)
		struct vec4 *colors = bmemdup(NULL, sizeof(struct vec4) * 6);
		for (int c = 0; c < 6; c++) {
			colors[c] = face_colors[i];
		}
		vbd.colors = colors;

		vertexbuffers_faces[i] = gs_vertexbuffer_create(&vbd, 0);

		// Array de 6 arrays de índices, cada uno para una cara
		static const uint16_t cube_face_indices[6][6] = {
			{0, 1, 3, 3, 2, 0}, // Cara 0
			{4, 6, 7, 7, 5, 4}, // Cara 1
			{0, 2, 6, 6, 4, 0}, // Cara 2
			{1, 5, 7, 7, 3, 1}, // Cara 3
			{2, 3, 7, 7, 6, 2}, // Cara 4
			{0, 4, 5, 5, 1, 0}  // Cara 5
		};

		// Al crear los index buffers:
		for (int i = 0; i < 6; i++) {
			uint16_t *indices_dup = bmemdup(cube_face_indices[i],
							sizeof(uint16_t) * 6);
			indexbuffers_faces[i] = gs_indexbuffer_create(
				GS_UNSIGNED_SHORT, indices_dup, 6, 0);
		}
	}

	obs_leave_graphics();
}

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

	data->texture = gs_texture_create(data->width, data->height, GS_RGBA, 1,
					  NULL, GS_RENDER_TARGET);

	blog(LOG_INFO, "create whiteboard texture %d %d", data->width,
	     data->height);

	obs_leave_graphics();
}
static void *cube_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct cube_filter_data *data =bzalloc(sizeof(struct cube_filter_data));
	data->source = source;
	data->pox = 0;
	data->posy = 0;
	//update_vertices();
	obs_enter_graphics();
	gs_render_start(true);


	create_face_buffers();
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

}

static void cube_filter_render(void *data, gs_effect_t *effect1)
{
	struct cube_filter_data *filter = (struct cube_filter_data *)data;


	// Obtener el efecto base por defecto
	gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (effect && filter->texture) {
		gs_blend_state_push();
		gs_reset_blend_state();
		gs_matrix_push();
		gs_matrix_identity();

		while (gs_effect_loop(effect, "Draw")) {
			
		obs_source_draw(filter->texture, 0, 0, 0, 0, false);
		}

		gs_matrix_pop();
		gs_blend_state_pop();
	}
	
}

static const char *cube_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused); // Macro común en OBS para evitar warnings
	return "Cubo 3D (Índices y UV, sin textura)";
}
static void cue_filter_tick(void* data, float seconds) {
	struct cube_filter_data *filter = data;
	struct obs_video_info video_info;

	filter->rotation_z += 45.0f * seconds;

	if (filter->rotation_z >= 360.0f)
		filter->rotation_z -= 360.0f;
	if (obs_get_video_info(&video_info)) {
		
			if (video_info.base_width != filter->width|| video_info.base_height != filter->height) {
			filter->width = video_info.base_width;
			filter->height = video_info.base_height;
			create_whiteboard_texture(filter);
		}
	}

	if (filter->texture == NULL) {
		blog("MAL", "MAL");
		return;
	}
	
	obs_enter_graphics();

	gs_texture_t *prev_render_target = gs_get_render_target();
	gs_texture_t *prev_zstencil_target = gs_get_zstencil_target();

	gs_set_render_target(filter->texture, NULL);
	gs_clear(GS_CLEAR_COLOR, (float[]){0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, 0);
	gs_viewport_push();
	gs_set_viewport(0, 0, filter->width, filter->height);
	gs_projection_push();
	
	gs_set_3d_mode(60.0f, 0.1f, 1000.0f);

	gs_blend_state_push();
	gs_reset_blend_state();

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	POINT mouse_pos;
	if (GetCursorPos(&mouse_pos)) {
		// mouse_pos.x y mouse_pos.y contienen la posición del cursor en pantalla
	} else {
		// Error al obtener la posición del cursor
	} 
	
	float dx = (float)mouse_pos.x;
	float dy = (float)mouse_pos.y;

	float len = sqrt(dx*dx + dy*dy); // longitud arbitraria para escalar la línea
	float angle = atan2f(dy, dx); // ángulo entre (0,0) y (dx, dy)


	//gs_set_3d_mode(60.0, 1.0, 1000.0);
	gs_enable_depth_test(true);
	gs_depth_function(GS_LESS);
	
	//gs_depth_function()// con esta función una vez detectados los pixeles decides cual quieres pintar sobre cual, recibe el enum de :
						//->el menor
						//->el menor igual	
						//->el mayor....
	gs_matrix_push();
	gs_matrix_identity();
	
	// Mueve el sistema de coordenadas a la posición del cursor
	gs_matrix_translate3f(filter->pox+500, filter->posy+500, 0.0f);

	

	gs_matrix_translate3f(size/2, size/2, -size/2); // centro del cuadrado

	// Rota y escala la línea para que apunte hacia el cursor
	float angle_rad = filter->rotation_z * (float)M_PI / 180.0f;
	gs_matrix_rotaa4f(1.0f, 1.0f, 0.0f, angle_rad);
	//gs_matrix_translate3f(0.0f, -size, 0.0f);
	gs_matrix_translate3f(-size / 2, -size / 2,size / 2); // centro del cuadrado
	gs_matrix_scale3f(1.0f, 1.0f, 1.0f);


	
	for (int face = 0; face < 6; face++) {
		gs_effect_set_vec4(color_param,&face_colors[face]); // Setea el color para esta cara
		gs_load_vertexbuffer(vertexbuffers_faces[face]);
		gs_load_indexbuffer(indexbuffers_faces[face]);
		gs_draw(GS_TRIS, 0, 6);
	}

	
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_matrix_pop();
	gs_enable_depth_test(false);
	// Luego, ya fuera de la matriz, restauras estados gráficos
	gs_projection_pop();
	gs_viewport_pop();
	gs_blend_state_pop();

	gs_set_render_target(prev_render_target, prev_zstencil_target);

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





