#include <obs-module.h>
#include "graphics/graphics.h"
#include "graphics/effect.h"
#include "graphics/vec4.h"

#include "graphics/quat.h"
#include <obs.h>
#include <plugin-support.h>
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cube", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Cubo con índice y UV, sin textura";
}

struct cube_filter_data {
	obs_source_t *context;
	gs_vertbuffer_t *vb;
	gs_indexbuffer_t *ib;
	gs_texture_t *text;
	int width, height;
};


static const struct vec3 cube_positions[] = {
	{-0.5f, -0.5f, 0.5f},
	{0.5f, -0.5f, 0.5f},
	{0.5f, 0.5f, 0.5f},
	{-0.5f, 0.5f, 0.5f},
};


static const struct vec2 cube_uvs[] = {
	{0.0f, 1.0f},
	{1.0f, 1.0f},
	{1.0f, 0.0f},
	{0.0f, 0.0f},
};


static const uint16_t cube_indices[] = {0, 1, 2, 2, 3, 0};
struct cube_filter_data *f;
static void render_cubo_3d(void)
{
	blog(LOG_INFO, "[CUBE] Renderizando cubo 3D");

	// Lista de posiciones de los vértices del cubo (8 vértices)
	float cube_positions[8][3] = {
		{-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
		{1.0f, 1.0f, -1.0f},   {-1.0f, 1.0f, -1.0f},
		{-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f}};

	// Índices que definen las caras del cubo (dos triángulos por cara)
	int cube_faces[6][6] = {
		{0, 1, 2, 0, 2, 3}, // Cara frontal
		{4, 5, 6, 4, 6, 7}, // Cara posterior
		{3, 2, 6, 3, 6, 7}, // Cara superior
		{0, 1, 5, 0, 5, 4}, // Cara inferior
		{1, 2, 6, 1, 6, 5}, // Cara derecha
		{0, 3, 7, 0, 7, 4}  // Cara izquierda
	};

	// Iniciar el renderizado
	gs_render_start(true);

	// Recorrer cada cara
	for (int i = 0; i < 6; i++) {
		// Dibujar los dos triángulos de cada cara
		for (int j = 0; j < 6; j += 3) {
			int idx1 = cube_faces[i][j];
			int idx2 = cube_faces[i][j + 1];
			int idx3 = cube_faces[i][j + 2];

			gs_vertex3f(cube_positions[idx1][0],
					cube_positions[idx1][1],
					cube_positions[idx1][2]);
			gs_vertex3f(cube_positions[idx2][0],
					cube_positions[idx2][1],
					cube_positions[idx2][2]);
			gs_vertex3f(cube_positions[idx3][0],
					cube_positions[idx3][1],
					cube_positions[idx3][2]);
		}
	}

	
}
static struct filter_data {
	obs_source_t *source;
	//gs_texture_t *text;
};
static void *cube_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct filter_data *filter =bzalloc(sizeof(struct filter_data)); // Asignar memoria
	filter->source = source; // Asociamos la fuente al filtro
	
	
	
	//blog(LOG_INFO, "[CUBE] Iniciando creación de filtro");

	//f = bzalloc(sizeof(*f));
	//if (!f) {
	//	blog(LOG_ERROR,
	//	     "[CUBE] Error al asignar memoria para el filtro");
	//	return NULL;
	//}
	//f->context = source;

	//f->width = 1920;
	//f->height = 1080;

	//blog(LOG_INFO, "[CUBE] Creando gs_vb_data");

	struct gs_vb_data *vd = gs_vbdata_create();
	if (!vd) {
		blog(LOG_ERROR, "[CUBE] Error al crear gs_vb_data");
		bfree(f);
		return NULL;
	}
	vd->num = 4;
	vd->points = bmemdup(cube_positions, sizeof(cube_positions));
	vd->num_tex = 1;
	vd->tvarray = bzalloc(sizeof(struct gs_tvertarray));
	vd->tvarray[0].array = bmemdup(cube_uvs, sizeof(cube_uvs));

	//blog(LOG_INFO, "[CUBE] Creando vertex buffer");

	f->vb = gs_vertexbuffer_create(vd, 0);
	if (!f->vb) {
		blog(LOG_ERROR, "[CUBE] Error al crear vertex buffer");
		gs_vbdata_destroy(vd);
		bfree(f);
		return NULL;
	}

	//blog(LOG_INFO, "[CUBE] Creando index buffer");

	///*f->ib = gs_indexbuffer_create(GS_UNSIGNED_SHORT, cube_indices,
	//			      sizeof(cube_indices), 0);
	//if (!f->ib) {
	//	blog(LOG_ERROR, "[CUBE] Error al crear index buffer");
	//	gs_vertexbuffer_destroy(f->vb);
	//	gs_vbdata_destroy(vd);
	//	bfree(f);
	//	return NULL;
	//}*/


	//blog(LOG_INFO, "[CUBE] Creando textura");
	//uint32_t color = 0xFF0000FF; // Rojo (0xRRGGBBAA)

	//// Convertir el color a un arreglo de bytes, de forma que coincida con el formato de textura esperado
	//uint8_t px[4]; // Un arreglo para los 4 bytes de RGBA

	//// Desglosar el color en componentes individuales de RGBA
	//px[0] = (color >> 24) & 0xFF; // Rojo
	//px[1] = (color >> 16) & 0xFF; // Verde
	//px[2] = (color >> 8) & 0xFF;  // Azul
	//px[3] = color & 0xFF;         // Alfa

	//// Crear un puntero a un arreglo de punteros a datos de textura
	//const uint8_t *data[1] = {px}; // Un solo puntero para la textura de 1x1

	//// Crear la textura de 1x1 con el color especificado
	//filter->text = gs_texture_create(1, 1, GS_RGBA, 1, data, 0);
	//gs_cubetexture_create(1, GS_CS_SRGB, 0, dat)
	//if (!filter->text) {
	//	blog(LOG_ERROR, "[CUBE] Error al crear textura");
	///*	gs_vertexbuffer_destroy(f->vb);
	//	gs_indexbuffer_destroy(f->ib);
	//	gs_vbdata_destroy(vd);*/
	//	bfree(filter);
	//	return NULL;
	//}

	//blog(LOG_INFO, "[CUBE] Filtro creado correctamente");

	//return f;
}

static void cube_filter_destroy(void *data)
{
	//struct cube_filter_data *f = data;

	//blog(LOG_INFO, "[CUBE] Destruyendo filtro");

	//if (f->vb) {
	//	blog(LOG_INFO, "[CUBE] Destruyendo vertex buffer");
	//	gs_vertexbuffer_destroy(f->vb);
	//}

	//if (f->ib) {
	//	blog(LOG_INFO, "[CUBE] Destruyendo index buffer");
	//	gs_indexbuffer_destroy(f->ib);
	//}

	//if (f->text) {
	//	blog(LOG_INFO, "[CUBE] Destruyendo textura");
	//	gs_texture_destroy(f->text);
	//}

	//bfree(f);
	//blog(LOG_INFO, "[CUBE] Filtro destruido");
}

static void cube_filter_render(void *data, gs_effect_t *effect1)
{

//UNUSED_PARAMETER(data);
//	gs_blend_state_push();
//	if (!obs_source_process_filter_begin(effect, GS_RGBA,
//						 OBS_ALLOW_DIRECT_RENDERING))
//		return;
//	
//
//	// 🔹 Paso 2: Dibuja un cuadrado rojo encima
//	gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
//	if (!default_effect)
//		return;
//
//	// Establecer el color rojo
//	float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
//	gs_eparam_t *color_param =
//		gs_effect_get_param_by_name(default_effect, "color");
//	if (color_param)
//		gs_effect_set_vec4(color_param, (const struct vec4 *)color);
//
//	gs_technique_t *tech = gs_effect_get_technique(default_effect, "Draw");
//	if (!tech)
//		return;
//
//	gs_technique_begin(tech);
//	while (gs_technique_begin_pass(tech, 0)) {
//		// 💡 Aquí defines el tamaño y posición del cuadrado rojo
//		gs_draw_sprite(
//			NULL, 0, 200,
//			200); // cuadrado de 200x200 en la esquina sup izq
//		gs_technique_end_pass(tech);
//	}
//	gs_technique_end(tech);
//	gs_blend_state_pop();
		
	struct filter_data *filter = data;

	blog(LOG_INFO, "[CUBE] Iniciando renderizado del filtro");

	obs_source_t *target = obs_filter_get_target(filter->source);
	if (target) {
		blog(LOG_INFO, "[CUBE] Renderizando fuente objetivo");
		obs_source_video_render(target);
	} else {
		blog(LOG_INFO, "[CUBE] Fuente objetivo no encontrada");
	}

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!effect) {
		blog(LOG_ERROR, "[CUBE] No se pudo obtener efecto base");
		return;
	}
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	if (!tech) {
		blog(LOG_ERROR, "[CUBE] No se pudo obtener técnica 'Draw'");
		return;
	}

	//// Crear cubemap
	//const uint32_t num_faces = 6;
	//const enum gs_color_format format = GS_RGBA;
	//const uint32_t levels = 1;
	//const uint32_t flags = GS_DYNAMIC;
	//const size_t face_size = 32 * 32 * 4;

	//blog(LOG_INFO, "[CUBE] Reservando memoria para caras de cubemap");

	//uint8_t *faces[6];
	//for (uint32_t i = 0; i < num_faces; ++i) {
	//	faces[i] = (uint8_t *)malloc(face_size);
	//	if (!faces[i]) {
	//		blog(LOG_ERROR,
	//			 "[CUBE] Fallo al reservar memoria para la cara %u", i);
	//		for (uint32_t j = 0; j < i; ++j)
	//			free(faces[j]);
	//		return;
	//	}
	//	for (uint32_t px = 0; px < 32 * 32; ++px) {
	//		faces[i][px * 4 + 0] = 255; // R
	//		faces[i][px * 4 + 1] = 0;   // G
	//		faces[i][px * 4 + 2] = 0;   // B
	//		faces[i][px * 4 + 3] = 255; // A
	//	}
	//}
	//blog(LOG_INFO, "[CUBE] Caras del cubemap preparadas");

	//const uint8_t *face_data[6];
	//for (uint32_t i = 0; i < num_faces; ++i)
	//	face_data[i] = faces[i];

	//gs_texture_t *cubemap =gs_texture_create(32,32, format, levels, face_data, flags);
	//if (!cubemap) {
	//	blog(LOG_ERROR, "[CUBE] Error al crear textura cúbica");
	//	for (uint32_t i = 0; i < num_faces; ++i)
	//		free(faces[i]);
	//	return;
	//}
	//blog(LOG_INFO, "[CUBE] Cubemap creado correctamente");

	/*for (uint32_t i = 0; i < num_faces; ++i)
		free(faces[i]);*/

	obs_enter_graphics();
	gs_set_3d_mode(1920, 1080, 90);
	blog(LOG_INFO, "[CUBE] Modo 3D establecido");
	gs_texture_t *render_target = gs_get_render_target();
	gs_zstencil_t *zsentil = gs_get_zstencil_target();

	gs_set_viewport(0, 0, 1920, 1080);
	gs_projection_push();
	gs_ortho(0, 1920, 0, 1080, 0.0, 1.0);
	gs_blend_state_push();
	gs_reset_blend_state();
	

	blog(LOG_INFO, "[CUBE] Matriz de transformación inicializada");
	
	if (gs_technique_begin(tech)) {
		if (gs_technique_begin_pass(tech, 0)) {
		/*struct quat rotation;
		quat_identity(&rotation);

		blog(LOG_INFO, "[CUBE] Dibujando cubo con backdrop");

		float left = -100.0f;
		float right = 100.0f;
		float top = 100.0f;
		float bottom = -100.0f;
		float znear = 0.6f;

		gs_draw_cube_backdrop(cubemap, &rotation, left, right, top,
				      bottom, znear);
		gs_draw_sprite(cubemap, 0, 32, 32);
		gs_technique_end_pass(tech);*/
		
		int width = 1920;  // Ancho de la ventana
		int height = 1080; // Alto de la ventana

		blog(LOG_INFO, "[CUBE] Configurando matrices de proyección y vista");

		// Guardar la matriz actual
		gs_matrix_push();
		gs_matrix_identity();
		gs_matrix_translate3f((float)(width / 2), (float)(height / 2), 0.0f);

		blog(LOG_INFO, "[CUBE] Cargando vertex buffer");
		render_cubo_3d();
		//gs_load_vertexbuffer(f->vb);
		gs_draw(GS_TRIS, 0, 32);
	

		gs_technique_end_pass(tech);

		blog(LOG_INFO, "[CUBE] Técnica finalizada correctamente");
	} else {
		blog(LOG_INFO ,"[CUBE] No se pudo iniciar el pase de técnica");
	}
		gs_technique_end(tech);
	} else {
		blog(LOG_INFO, "[CUBE] No se pudo iniciar la técnica");
	}

	gs_matrix_pop();
	// Restaurar el viewport
	gs_viewport_pop();
	// Restaurar el estado de mezcla
	gs_blend_state_pop();
	obs_leave_graphics();
	blog(LOG_INFO, "[CUBE] Matriz restaurada y renderizado completado");
		// Restaura el estado anterior de la matriz
	//gs_matrix_pop();

	// Aplica una rotación al cubo (por ejemplo, 45 grados alrededor del eje Y)
	//gs_matrix_rotate_y(45.0f * (float)M_PI / 180.0f);
	
	// Aplica una traslación si deseas mover el cubo a otra posición
	// gs_matrix_translate3f(x, y, z);
	
	// Crea y dibuja el cubo con dimensiones 1x1x1
	
	//
	//	// Paso 2: Dibujar un cuadrado rojo encima
	
//
//	// Establecer el color rojo (r=1, g=0, b=0, alpha=1)
//	gs_eparam_t *color_param =
//		gs_effect_get_param_by_name(default_effect, "color");
//	if (color_param) {
//		float color[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // Rojo
//		gs_effect_set_vec4(color_param, (const struct vec4 *)color);
//	}
//
//	// Iniciar la técnica de dibujo
//	gs_technique_begin(tech);
//	// Usamos un solo pase para dibujar el cuadrado
//	gs_technique_begin_pass(tech, 0);
//
//	// Dibujar el cuadrado de 200x200 en la posición (100, 100)
//	gs_draw_sprite(filter->text, 0, 100, 100); // Cuadrado rojo de 200x200 px
//	/*gs_draw_cube_backdrop
//		gs_draw_*/
//	// Finalizamos el pase de la técnica
//	gs_technique_end_pass(tech);
//	gs_technique_end(tech);
//
//// renderiza la cámara original
//	//blog(LOG_INFO, "[CUBE] Renderizando cubo 3D");

	//obs_enter_graphics();


	//gs_texture_t *render_target = gs_get_render_target();
	//gs_zstencil_t *zsentil = gs_get_zstencil_target();

	//gs_set_viewport(0, 0, f->width, f->height);
	//gs_projection_push();
	//gs_ortho(0, f->width, 0, f->height, 0.0, 1.0);
	//gs_blend_state_push();
	//gs_reset_blend_state();
	//gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);

	//// Obtener el parámetro 'color' del efecto
	//gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");

	//// Obtener la técnica 'Solid' del efecto
	//gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	//// Iniciar la técnica
	//gs_technique_begin(tech);

	//// Iniciar la pasada de la técnica
	//gs_technique_begin_pass(tech, 0);

	//obs_source_t *source = obs_get_source_by_name("Video Capture Device");

	//int width = 1920;  // Ancho de la ventana
	//int height = 1080; // Alto de la ventana

	//blog(LOG_INFO, "[CUBE] Configurando matrices de proyección y vista");

	//// Guardar la matriz actual
	//gs_matrix_push();
	//gs_matrix_identity();
	//gs_matrix_translate3f((float)(width / 2), (float)(height / 2), 0.0f);

	//blog(LOG_INFO, "[CUBE] Cargando vertex buffer");

	//gs_load_vertexbuffer(f->vb);
	//gs_draw(GS_TRIS, 0, 4);
	//gs_matrix_pop();

	//gs_technique_end_pass(tech);
	//gs_technique_end(tech);

	//gs_matrix_pop();
	//gs_projection_pop();

	//// Restaurar el viewport
	//gs_viewport_pop();
	//// Restaurar el estado de mezcla
	//gs_blend_state_pop();

	//// Restaurar el render target anterior
	//gs_set_render_target(render_target, zsentil);

	//obs_leave_graphics();

	//blog(LOG_INFO, "[CUBE] Renderizado completado");
}

static const char *cube_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused); // Macro común en OBS para evitar warnings
	return "Cubo 3D (Índices y UV, sin textura)";
}

static struct obs_source_info cube_filter = {
	.id = "cube_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = cube_filter_get_name,
	.create = cube_filter_create,
	.destroy = cube_filter_destroy,
	.video_render = cube_filter_render,
};

bool obs_module_load(void)
{
	blog(LOG_INFO, "[CUBE] Registrando filtro");

	obs_register_source(&cube_filter);

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
