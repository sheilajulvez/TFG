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



//gs_vertbuffer_t *vb;

//static const uint16_t cube_indices[] = {0, 1, 2, 2, 3, 0};
//struct cube_filter_data *f;
//static void render_cubo_3d(void)
//{
//	blog(LOG_INFO, "[CUBE] Renderizando cubo 3D");
//
//	// Lista de posiciones de los vértices del cubo (8 vértices)
//	float cube_positions[8][3] = {
//		{-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
//		{1.0f, 1.0f, -1.0f},   {-1.0f, 1.0f, -1.0f},
//		{-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},
//		{1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f}};
//
//	// Índices que definen las caras del cubo (dos triángulos por cara)
//	int cube_faces[6][6] = {
//		{0, 1, 2, 0, 2, 3}, // Cara frontal
//		{4, 5, 6, 4, 6, 7}, // Cara posterior
//		{3, 2, 6, 3, 6, 7}, // Cara superior
//		{0, 1, 5, 0, 5, 4}, // Cara inferior
//		{1, 2, 6, 1, 6, 5}, // Cara derecha
//		{0, 3, 7, 0, 7, 4}  // Cara izquierda
//	};
//
//	// Iniciar el renderizado
//	gs_render_start(true);
//
//	// Recorrer cada cara
//	for (int i = 0; i < 6; i++) {
//		// Dibujar los dos triángulos de cada cara
//		for (int j = 0; j < 6; j += 3) {
//			int idx1 = cube_faces[i][j];
//			int idx2 = cube_faces[i][j + 1];
//			int idx3 = cube_faces[i][j + 2];
//
//			gs_vertex3f(cube_positions[idx1][0],
//					cube_positions[idx1][1],
//					cube_positions[idx1][2]);
//			gs_vertex3f(cube_positions[idx2][0],
//					cube_positions[idx2][1],
//					cube_positions[idx2][2]);
//			gs_vertex3f(cube_positions[idx3][0],
//					cube_positions[idx3][1],
//					cube_positions[idx3][2]);
//		}
//	}
//
//	
//}
//static struct filter_data {
//	obs_source_t *source;
//	//gs_texture_t *text;
//};
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
	//bfree(filter);
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
	/*filter->pox = filter->pox + vel * seconds;
	filter->posy = filter->posy + vel * seconds;*/
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
	//update_vertices();
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

	
	//struct vec4 color_v4;
	//vec4_from_rgba(&color_v4, 0XFFFF0080); // ejemplo color blanco
	//gs_effect_set_vec4(color_param, &color_v4);
	//

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
	gs_depth_function(GS_ALWAYS);
	
	//gs_depth_function()// con esta función una vez detectados los pixeles decides cual quieres pintar sobre cual, recibe el enum de :
						//->el menor
						//->el menor igual	
						//->el mayor....
	gs_matrix_push();
	gs_matrix_identity();
	
	// Mueve el sistema de coordenadas a la posición del cursor
	gs_matrix_translate3f(filter->pox+500, filter->posy+500, 0.0f);

	//gs_matrix_push();
	////gs_matrix_translate3f(-size, -size, 0.0f);
	//// Aquí podrías dibujar algo, si quieres, por ejemplo un punto o la "cabeza" de la línea
	//// Pero si no hay dibujo aquí, no hace falta este push/pop

	//gs_matrix_pop();
	// 2. Traslada al centro del objeto
	gs_matrix_translate3f(size/2, size/2, -size/2); // centro del cuadrado

	// Rota y escala la línea para que apunte hacia el cursor
	float angle_rad = filter->rotation_z * (float)M_PI / 180.0f;
	gs_matrix_rotaa4f(1.0f, 1.0f, 0.0f, angle_rad);
	//gs_matrix_translate3f(0.0f, -size, 0.0f);
	gs_matrix_translate3f(-size / 2, -size / 2,size / 2); // centro del cuadrado
	gs_matrix_scale3f(1.0f, 1.0f, 1.0f);

	//// Carga el buffer de vértices y dibuja la línea
	///*for (int i = 0; i < 6; i++) {
	//	gs_effect_set_vec4(color_param, &cube_colors[i/2]);
	//	gs_load_vertexbuffer(cube_faces[i]);
	//	gs_draw(GS_TRIS, 0, 36);
	//}*/
	//gs_matrix_scale3f(2, 2, 2);
	//gs_load_vertexbuffer(vertexbuffer);
	//gs_load_indexbuffer(indexbuffer);
	//gs_draw(GS_TRIS, 0, 36);
	const struct vec4 cube_colors[6] = {
		{1, 0, 0, 1}, // Rojo
		{0, 1, 0, 1}, // Verde
		{0, 0, 1, 1}, // Azul
		{1, 1, 0, 1}, // Amarillo
		{1, 0, 1, 1}, // Magenta
		{0, 1, 1, 1}, // Cyan
	};

	for (int i = 0; i < 6; i++) {
		gs_effect_set_vec4(color_param, &cube_colors[i]);

		gs_load_vertexbuffer(vertexbuffer);
		gs_load_indexbuffer(indexbuffer);

		// Cada cara son 6 índices (2 triángulos)
		gs_draw(GS_TRIS, i * 6, 6);
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






//#include <obs-module.h>
//#include <graphics/graphics.h>
//
//OBS_DECLARE_MODULE()
//OBS_MODULE_USE_DEFAULT_LOCALE("cube_filter", "en-US")
//
//// 1. Estructura de datos del filtro
//struct cube_filter_data {
//	obs_source_t *context;
//	gs_vertbuffer_t *vb;
//	gs_indexbuffer_t *ib;
//	gs_texture_t *tex;
//};
//
//// 2. Definición de vértices y UVs (24 vértices únicos)
//struct vertex {
//	struct vec3 pos;
//	struct vec2 uv;
//};
//
//static const struct vertex cube_verts[24] = {
//	// Frente
//	{{-1, -1, 1}, {0, 1}},
//	{{1, -1, 1}, {1, 1}},
//	{{1, 1, 1}, {1, 0}},
//	{{-1, 1, 1}, {0, 0}},
//	// Atrás
//	{{1, -1, -1}, {0, 1}},
//	{{-1, -1, -1}, {1, 1}},
//	{{-1, 1, -1}, {1, 0}},
//	{{1, 1, -1}, {0, 0}},
//	// Izquierda
//	{{-1, -1, -1}, {0, 1}},
//	{{-1, -1, 1}, {1, 1}},
//	{{-1, 1, 1}, {1, 0}},
//	{{-1, 1, -1}, {0, 0}},
//	// Derecha
//	{{1, -1, 1}, {0, 1}},
//	{{1, -1, -1}, {1, 1}},
//	{{1, 1, -1}, {1, 0}},
//	{{1, 1, 1}, {0, 0}},
//	// Superior
//	{{-1, 1, 1}, {0, 1}},
//	{{1, 1, 1}, {1, 1}},
//	{{1, 1, -1}, {1, 0}},
//	{{-1, 1, -1}, {0, 0}},
//	// Inferior
//	{{-1, -1, -1}, {0, 1}},
//	{{1, -1, -1}, {1, 1}},
//	{{1, -1, 1}, {1, 0}},
//	{{-1, -1, 1}, {0, 0}},
//};
//
//// 3. Índices para 12 triángulos (36 índices)
//static const uint16_t cube_idx[36] = {
//	0,  1,  2,  2,  3,  0,  // frente
//	4,  5,  6,  6,  7,  4,  // atrás
//	8,  9,  10, 10, 11, 8,  // izquierda
//	12, 13, 14, 14, 15, 12, // derecha
//	16, 17, 18, 18, 19, 16, // superior
//	20, 21, 22, 22, 23, 20  // inferior
//};
//
//// 4. Crear vertex/index buffers y textura
//static void *cube_filter_create(obs_data_t *settings, obs_source_t *source)
//{
//	struct cube_filter_data *f = bmalloc(sizeof(*f));
//	f->context = source;
//
//	// 1) Creamos el gs_vb_data
//	struct gs_vb_data *vd = gs_vbdata_create();
//	vd->num = 24; // 24 vértices
//	vd->points = bmemdup(cube_verts, sizeof(cube_verts));
//	vd->num_tex = 1; // una capa de UVs
//
//	// 2) Extraemos los UVs a un array puro
//	struct vec2 *uvs = bmalloc(sizeof(struct vec2) * 24);
//	for (size_t i = 0; i < 24; ++i)
//		uvs[i] = cube_verts[i].uv;
//
//	// 3) Indicamos cuántos UVs y asignamos el puntero
//	vd->tvarray[0].width = 24;
//	vd->tvarray[0].array = uvs;
//
//	// 4) Creamos el vertex buffer y liberamos vd
//	f->vb = gs_vertexbuffer_create(vd, 0);
//	gs_vbdata_destroy(vd);
//
//	// 5) Creamos el index buffer
//	f->ib = gs_indexbuffer_create(
//		GS_UNSIGNED_SHORT, bmemdup(cube_idx, sizeof(cube_idx)), 36, 0);
//
//	// 6) Creamos la textura 1×1 azul
//	uint32_t px = 0xFF0000FF;
//	f->tex = gs_texture_create(1, 1, GS_RGBA, 1, (const uint8_t **)&px, 0);
//
//	return f;
//}
//
//// 5. Destruir recursos
//static void cube_filter_destroy(void *data)
//{
//	struct cube_filter_data *f = data;
//	gs_vertexbuffer_destroy(f->vb); // liberar buffers
//	gs_indexbuffer_destroy(f->ib);  // liberar buffers
//	gs_texture_destroy(f->tex);     // liberar textura
//	bfree(f);                       // liberar memoria del filtro
//}
//
//// 6. Renderizar el cubo
//static void cube_filter_render(void *data, gs_effect_t *effect)
//{
//	struct cube_filter_data *f = data;
//
//	// 6.1. Render de la fuente original
//	obs_source_t *target = obs_filter_get_target(f->context);
//	if (target)
//		obs_source_video_render(target);
//
//	// 6.2. Estado 3D
//	gs_enable_depth_test(true);
//	gs_set_cull_mode(GS_BACK);
//
//	// 6.3. Proyección perspectiva
//	gs_matrix_push();
//	gs_projection_push();
//	int w = obs_source_get_base_width(f->context);
//	int h = obs_source_get_base_height(f->context);
//	float asp = (float)w / (float)h;
//	gs_perspective(45.0f, asp, 0.1f, 100.0f);
//
//	// 6.4. Transformaciones y rotaciones
//	gs_matrix_translate3f(0.0f, 0.0f, -5.0f);
//	double t = (double)os_gettime_ns() / 1e9;
//	gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, (float)t);
//	gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, (float)(t * 0.5));
//
//	// 6.5. Carga de buffers y textura
//	gs_load_vertexbuffer(f->vb);
//	gs_load_indexbuffer(f->ib);
//	gs_load_texture(f->tex, 0);
//
//	// 6.6. Dibujar 12 triángulos (36 vértices)
//	gs_draw(GS_TRIS, 0, 36);
//
//	// 6.7. Restaurar matrices y desactivar profundidad
//	gs_projection_pop();
//	gs_matrix_pop();
//	gs_enable_depth_test(false);
//}
//
//static const char *cube_filter_get_name(void *unused)
//{
//	UNUSED_PARAMETER(unused);
//	return "Cubo 3D (GS Draw)";
//}
//
//// 7. Registro del filtro
//static struct obs_source_info cube_filter_info = {
//	.id = "cube_filter",
//	.type = OBS_SOURCE_TYPE_FILTER,
//	.output_flags = OBS_SOURCE_VIDEO,
//	.get_name = cube_filter_get_name,
//	.create = cube_filter_create,
//	.destroy = cube_filter_destroy,
//	.video_render = cube_filter_render,
//};
//
//bool obs_module_load(void)
//{
//	obs_register_source(&cube_filter_info);
//	return true;
//}
