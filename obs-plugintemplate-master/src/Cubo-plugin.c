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

// For M_PI on Windows
#define _USE_MATH_DEFINES
#include <math.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cube", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "SJ_3D";
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
	char *texture_path_str;
	float scale;
	float rotation_y_slider_value;
	float rotation_x_slider_value;
	float rotation_z_slider_value;
	float current_rotation_z_angle;
	float current_rotation_x_angle;
	float current_rotation_y_angle;
	struct Mesh *g_meshes;
	size_t g_mesh_count;
};



static uint32_t cube_source_get_width(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->width;
}

static uint32_t cube_source_get_height(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->height;
}
static gs_texture_t *load_texture_file(const char *path)
{
	if (!path || strlen(path) == 0) {
		return NULL;
	}

	obs_enter_graphics();
	gs_image_file_t image;
	gs_image_file_init(&image, path);
	gs_image_file_init_texture(&image);
	obs_leave_graphics();

	if (image.loaded) {
		blog(LOG_INFO, "Textura de usuario cargada: %s", path);
		gs_texture_t *new_texture = image.texture;
		gs_image_file_free(
			&image); // Libera los datos internos de la imagen, no la textura
		return new_texture;
	} else {
		if (image.texture) { // Si hubo un intento de crear la textura pero falló la carga
			gs_texture_destroy(image.texture);
		}
		blog(LOG_WARNING, "No se pudo cargar la textura de usuario: %s",
		     path);
		gs_image_file_free(
			&image); // Asegúrate de liberar la estructura de imagen
		return NULL;
	}
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

void create_whiteboard_texture(struct cube_filter_data *data)
{
	obs_enter_graphics();

	if (data->texture != NULL) {
		gs_texture_destroy(data->texture);
		data->texture = NULL;
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
	UNUSED_PARAMETER(unused);
	return "SJ_3D";
}

static void *cube_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct cube_filter_data *data =
		bzalloc(sizeof(struct cube_filter_data));
	data->source = source;
	data->g_meshes = NULL;
	data->g_mesh_count = 0;

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		data->width = ovi.base_width;
		data->height = ovi.base_height;
		create_whiteboard_texture(data);
	} else {
		blog(LOG_WARNING, "Whiteboard: Failed to get video resolution");
	}

	return data;
}

static void cube_filter_destroy(void *data)
{
	blog(LOG_WARNING, "CERRANDO");
	struct cube_filter_data *filter = (struct cube_filter_data *)data;
	cleanup_global_meshes(&filter->g_meshes, &filter->g_mesh_count);
	obs_enter_graphics();
	if (filter->texture) {
		gs_texture_destroy(filter->texture);
	}
	if (filter->zstencil) {
		gs_zstencil_destroy(filter->zstencil);
	}
	obs_leave_graphics();
	bfree(filter->model_path_str);
	bfree(filter);
}

static void cube_filter_render(void *data, gs_effect_t *effect1)
{
	struct cube_filter_data *filter = (struct cube_filter_data *)data;

	obs_source_t *target = obs_filter_get_target(filter->source);
	if (target)obs_source_video_render(target);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (effect) {
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

static obs_properties_t *cube_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_group(props, "position_group",
				 obs_module_text("Posición"), OBS_GROUP_NORMAL,
				 props);
	obs_properties_add_float_slider(props, "pos_x",
					obs_module_text("Posición X"), -3000.0f,
					3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "pos_y",
					obs_module_text("Posición Y"), -3000.0f,
					3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "pos_z",
					obs_module_text("Posición Z"), -3000.0f,
					3000.0f, 10.0f);
	obs_properties_add_float_slider(
		props, "scale", obs_module_text("Escala"), 1, 1000, 1);
	obs_properties_add_float_slider(props, "rotation_z_slider_value",
					obs_module_text("Rotación Z (Grados)"),
					-360.0f, 360.0f, 1.0f);
	obs_properties_add_float_slider(props, "rotation_x_slider_value",
					obs_module_text("Rotación Y (Grados)"),
					-360.0f, 360.0f, 1.0f);
	obs_properties_add_float_slider(props, "rotation_y_slider_value",
					obs_module_text("Rotación X (Grados)"),
					-360.0f, 360.0f, 1.0f);
	obs_properties_add_path(
		props, "model_path", obs_module_text("Ruta del Modelo 3D"),
		OBS_PATH_FILE,
		"Modelos 3D (*.obj *.dae *.gltf *.blend);;Todos los archivos (*.*)",
		NULL);
	 obs_properties_add_path(
        props, "texture_path", obs_module_text("Ruta de la Textura (Opcional)"),
        OBS_PATH_FILE,
        "Archivos de Imagen (*.png *.jpg *.jpeg *.bmp *.tga);;Todos los archivos (*.*)",
        NULL);

	return props;
}

static void cube_filter_update(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	/*blog(LOG_INFO, "[CUBE][UPDATE] Valor recibido para 'scale': %f",
	     obs_data_get_double(settings, "scale"));
	blog(LOG_INFO, "[CUBE][UPDATE] Valor recibido para 'posx': %f",
	     obs_data_get_double(settings, "pos_x"));
	blog(LOG_INFO, "[CUBE][UPDATE] Valor recibido para 'pos_y': %f",
	     obs_data_get_double(settings, "`pos_y"));*/
	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
	filter->scale = (float)obs_data_get_double(settings, "scale");
	filter->rotation_x_slider_value =
		(float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y_slider_value =
		(float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z_slider_value =
		(float)obs_data_get_double(settings, "rotation_z_slider_value");
	const char *new_model_path_c_str =
		obs_data_get_string(settings, "model_path");

	filter->current_rotation_z_angle = filter->rotation_z_slider_value;
	filter->current_rotation_x_angle = filter->rotation_x_slider_value;
	filter->current_rotation_y_angle = filter->rotation_y_slider_value;
	 const char *new_texture_path_c_str = obs_data_get_string(settings, "texture_path"); 
	if (!filter->model_path_str ||
	    strcmp(filter->model_path_str, new_model_path_c_str) != 0 ||
	    filter->g_mesh_count == 0) {
		bfree(filter->model_path_str);
		filter->model_path_str = bstrdup(new_model_path_c_str);

		if (filter->model_path_str &&
		    strlen(filter->model_path_str) > 0) {
			blog(LOG_INFO, "Cargando nuevo modelo desde: %s",
			     filter->model_path_str);
			load_model_c(filter->model_path_str, &filter->g_meshes,
				     &filter->g_mesh_count);
		}
	}
	if (!filter->texture_path_str ||
	    strcmp(filter->texture_path_str, new_texture_path_c_str) != 0) {

		bfree(filter->texture_path_str);
		filter->texture_path_str = bstrdup(new_texture_path_c_str);

		gs_texture_t *text = load_texture_file(filter->texture_path_str);
		if (text) {
			apply_texture_to_all_meshes(filter->g_meshes,filter->g_mesh_count,text);
		}

	}
}

static void cube_filter_tick(void *data, float seconds)
{
	struct cube_filter_data *filter = data;
	struct obs_video_info video_info;

	if (obs_get_video_info(&video_info)) {
		if (video_info.base_width != filter->width ||
		    video_info.base_height != filter->height) {
			filter->width = video_info.base_width;
			filter->height = video_info.base_height;
			create_whiteboard_texture(filter);
		}
	}

	obs_enter_graphics();
	gs_render_start(true);
	gs_viewport_push();
	gs_set_viewport(0, 0, filter->width, filter->height);
	gs_projection_push();
	gs_set_3d_mode(60.0f, 0.01f, 5000);
	gs_blend_state_push();
	gs_reset_blend_state();

	gs_enable_depth_test(true);
	gs_depth_function(GS_LESS);

	gs_texture_t *prev_render_target = gs_get_render_target();
	gs_texture_t *prev_zstencil_target = gs_get_zstencil_target();
	gs_set_render_target(filter->texture, filter->zstencil);

	gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH,
		 (float[]){0.0f, 0.0f, 0.0f, 0.0f}, 1.0f, 0);
	gs_matrix_push();
	gs_matrix_identity();
	
	gs_matrix_translate3f(filter->pos_x, filter->pos_y, filter->pos_z);
	gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f,
			  filter->current_rotation_z_angle * (float)M_PI /
				  180.0f);
	gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f,
			  filter->current_rotation_x_angle * (float)M_PI /
				  180.0f);
	gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f,
			  filter->current_rotation_y_angle * (float)M_PI /
				  180.0f);
	gs_matrix_scale3f(filter->scale, filter->scale, filter->scale);
	render_model_c(filter->g_meshes, filter->g_mesh_count);
	gs_matrix_pop();
	gs_set_render_target(prev_render_target, prev_zstencil_target);

	gs_projection_pop();
	gs_viewport_pop();
	gs_blend_state_pop();
	gs_render_start(false);
	obs_leave_graphics();
}



static void cube_filter_save(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	blog(LOG_INFO, "[CUBE] Guardando valores");
	obs_data_set_double(settings, "pos_x", filter->pos_x);
	obs_data_set_double(settings, "pos_y", filter->pos_y);
	obs_data_set_double(settings, "pos_z", filter->pos_z);
	obs_data_set_double(settings, "scale", filter->scale);
	obs_data_set_double(settings, "rotation_x_slider_value",
			    filter->rotation_x_slider_value);
	obs_data_set_double(settings, "rotation_y_slider_value",
			    filter->rotation_y_slider_value);
	obs_data_set_double(settings, "rotation_z_slider_value",
			    filter->rotation_z_slider_value);

	if (filter->model_path_str != NULL)
		obs_data_set_string(settings, "model_path",
				    filter->model_path_str);
	
}
void cube_filter_load(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	blog(LOG_INFO, "[CUBE] Cargando valores");

	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
	filter->scale = (float)obs_data_get_double(settings, "scale");

	filter->rotation_x_slider_value =
		(float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y_slider_value =
		(float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z_slider_value =
		(float)obs_data_get_double(settings, "rotation_z_slider_value");

	const char *model_path = obs_data_get_string(settings, "model_path");
	if (model_path && *model_path != '\0') {
		filter->model_path_str = bstrdup(model_path);
	}
	cube_filter_update(data, settings);

}
static void cube_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "pos_x", 0.0);
	obs_data_set_default_double(settings, "pos_y", 0.0);
	obs_data_set_default_double(settings, "pos_z", 0.0);
	obs_data_set_default_double(settings, "scale", 100.0);
	obs_data_set_default_double(settings, "rotation_x_slider_value", 0.0);
	obs_data_set_default_double(settings, "rotation_y_slider_value", 0.0);
	obs_data_set_default_double(settings, "rotation_z_slider_value", 0.0);
	obs_data_set_default_string(settings, "model_path", "");
}

static struct obs_source_info cube_filter = {
	.id = "cube_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = cube_filter_get_name,
	.create = cube_filter_create,
	.destroy = cube_filter_destroy,
	.video_render = cube_filter_render,
	.video_tick = cube_filter_tick,
	.get_properties = cube_filter_properties,
	.update = cube_filter_update,
	.save = cube_filter_save,
	.get_defaults = cube_filter_defaults,
	.load=cube_filter_load
};
bool obs_module_load(void)
{
	blog(LOG_INFO, "[CUBE] Registrando filtro");
	obs_register_source(&cube_filter);
	blog(LOG_INFO, "[CUBE] Registrade");
	return true;
}