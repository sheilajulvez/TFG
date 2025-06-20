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
#include "SJ_3DModel.h"





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
	float pos_x;
	float pos_y;
	float pos_z; 
	char *model_path_str;
	float scale; 
	float rotation_y_slider_value;
	float rotation_x_slider_value;
	float rotation_z_slider_value; 
	float current_rotation_z_angle; 
	float current_rotation_x_angle;
	float current_rotation_y_angle;

	gs_texrender_t *texrender; //  Usa esto en su lugar
};

static uint32_t cube_source_get_width(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->width; // Retorna el ancho actual
}

static uint32_t cube_source_get_height(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->height; // Retorna la altura actual
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

//static gs_vertbuffer_t *cube_faces[6];

static gs_indexbuffer_t *indexbuffer; //triangulos 
static gs_vertbuffer_t *vertexbuffer; //vertices

// Vértices del cubo (8 vértices)
//struct vec3 cube_vertices[8] = {
//	{0, 0, 0},   // 0
//	{50, 0, 0},  // 1
//	{0, 50, 0},  // 2
//	{50, 50, 0}, // 3
//	{0, 0, 50},  // 4
//	{50, 0, 50}, // 5
//	{0, 50, 50}, // 6
//	{50, 50, 50} // 7
//};
//// Índices para las 12 caras del cubo (36 índices)
//static const uint16_t cube_indices[] = {
//    0, 1, 3,  3, 2, 0,  // Cara trasera  (Z=0)
//    4, 6, 7,  7, 5, 4,  // Cara delantera (Z=50)
//    0, 2, 6,  6, 4, 0,  // Cara izquierda (X=0)
//    1, 5, 7,  7, 3, 1,  // Cara derecha  (X=50)
//    2, 3, 7,  7, 6, 2,  // Cara superior (Y=50)
//    0, 4, 5,  5, 1, 0   // Cara inferior (Y=0)
//};

//struct cube_vertex {
//	struct vec3 pos;
//	uint32_t color;
//};

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
	blog(LOG_INFO, "create whiteboard texture %d %d", data->width,data->height);

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
	
	obs_enter_graphics();
	gs_render_start(true);


	obs_leave_graphics();

	//load_model_c(
		//"C:\\Users\\USER\\Downloads\\10450_Rectangular_Grass_Patch_L3.123c827d110a-1347-4381-9208-e4f735762647\\10450_Rectangular_Grass_Patch_L3.123c827d110a-1347-4381-9208-e4f735762647\\10450_Rectangular_Grass_Patch_v1_iterations-2.obj");
		//"C:/Users/USER/Downloads/89-1a/tazita.obj");
		//"C:/Users/USER/Downloads/we1nywlheigw-1/semtex.obj");
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
		//apply_textyre()
		while (gs_effect_loop(effect, "Draw")) {
			obs_source_draw(gs_texrender_get_texture(filter->texrender), 0, 0, filter->width, filter->height, false);
			obs_source_draw(filter->texture, 0, 0, 0, 0, false);
		}

		gs_matrix_pop();
		gs_blend_state_pop();
	}
	obs_leave_graphics();
}
static obs_properties_t *cube_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	// Grupo para posición
	// obs_properties_add_group necesita la cabecera <obs-properties.h>
	obs_properties_t *pos_group = obs_properties_add_group(props, "position_group", obs_module_text("Posición"),OBS_GROUP_NORMAL,props);
	obs_properties_add_float_slider(props, "pos_x",obs_module_text("Posición X"), -1000.0f,1000.0f, 1.0f);
	obs_properties_add_float_slider(props, "pos_y",obs_module_text("Posición Y"), -1000.0f,1000.0f, 1.0f);
	obs_properties_add_float_slider(props, "pos_z",obs_module_text("Posición Z"), -1000.0f,1000.0f, 1.0f); // Posición Z
	obs_properties_add_float_slider(props, "scale", obs_module_text("Escala"), 0.01f,10.0f,  0.01f); // Incremento más fino
	// Control de Rotación en Z
	obs_properties_add_float_slider(props, "rotation_z_slider_value",obs_module_text("Rotación Z (Grados)"),-360.0f, 360.0f, 1.0f); // Rotación en Z
	obs_properties_add_float_slider(props, "rotation_x_slider_value",obs_module_text("Rotación Y (Grados)"),-360.0f, 360.0f, 1.0f); // Rotación en X
	obs_properties_add_float_slider(props, "rotation_y_slider_value",obs_module_text("Rotación X (Grados)"),-360.0f, 360.0f, 1.0f); // Rotación en Y
	// Añadir control para la ruta del modelo (OPCIONAL, pero muy útil)
	obs_properties_add_path(props, "model_path", obs_module_text("Ruta del Modelo 3D"),OBS_PATH_FILE,"Modelos 3D (*.obj *.fbx *.dae *.gltf);;Todos los archivos (*.*)",NULL);

	return props;
}

static void cube_filter_update(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;

	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z =(float)obs_data_get_double(settings, "pos_z"); // Obtener Z
	filter->scale = (float)obs_data_get_double(settings, "scale");
	filter->rotation_x_slider_value =(float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y_slider_value =(float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z_slider_value =(float)obs_data_get_double(settings, "rotation_z_slider_value");
	const char *new_model_path_c_str =obs_data_get_string(settings, "model_path");
	// La rotación toma el valor directo del slider
	filter->current_rotation_z_angle = filter->rotation_z_slider_value;
	filter->current_rotation_x_angle = filter->rotation_x_slider_value;
	filter->current_rotation_y_angle = filter->rotation_y_slider_value;
	if (!filter->model_path_str ||
	    strcmp(filter->model_path_str, new_model_path_c_str) != 0) {

		// Liberar la ruta anterior si existe
		bfree(filter->model_path_str);

		// Duplicar la nueva ruta para almacenarla
		filter->model_path_str = bstrdup(new_model_path_c_str);

		// 3. Cargar el nuevo modelo si la ruta no está vacía
		if (filter->model_path_str && strlen(filter->model_path_str) > 0) {
			blog(LOG_INFO, "Cargando nuevo modelo desde: %s",filter->model_path_str);
			// Llama a tu función para cargar el modelo 3D
			// Debes asegurarte de que load_model_c libere cualquier modelo anterior.
			// Pasa la ruta al cargador del modelo.
			
			load_model_c(filter->model_path_str);
			
		} 
	}
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

	//// --- Cubo 1 TEXTURA
	gs_texture_t *prev_render_target = gs_get_render_target();
	gs_texture_t *prev_zstencil_target = gs_get_zstencil_target();
	gs_set_render_target(filter->texture, filter->zstencil); //TEXTURA
	
	
	gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH,(float[]){0.0f, 0.0f, 0.0f, 0.0f}, 1.0f, 0);
	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_translate3f(filter->pos_x, filter->pos_y, filter->pos_z); 
	gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f,filter->current_rotation_z_angle * (float)M_PI /180.0f); 
	gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f,filter->current_rotation_x_angle * (float)M_PI /180.0f); 
	gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f,filter->current_rotation_y_angle * (float)M_PI /180.0f); 
	gs_matrix_scale3f(filter->scale, filter->scale, filter->scale);
	render_model_c();
	gs_matrix_pop();
	gs_set_render_target(prev_render_target, prev_zstencil_target);

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
	.get_properties = cube_filter_properties,
	.update = cube_filter_update,
	
};

bool obs_module_load(void)
{
	blog(LOG_INFO, "[CUBE] Registrando filtro");

	obs_register_source(&cube_filter);
	blog(LOG_INFO, "[CUBE] Registrade");
	return true;
}



