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


#include "aruco_detector.h"

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
	float width_screen; 
	float height_screen;
	float *model_width;
	float * model_height;
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
	struct Mesh *g_meshes;
	ArucoResult last_result;
	size_t g_mesh_count;


};
#include <stdint.h>

// Utilidad para convertir un píxel YUV a BGRA
static void yuv_to_bgra(uint8_t y, uint8_t u, uint8_t v, uint8_t *b, uint8_t *g,uint8_t *r)
{
	int c = y - 16;
	int d = u - 128;
	int e = v - 128;

	int r_ = (298 * c + 409 * e + 128) >> 8;
	int g_ = (298 * c - 100 * d - 208 * e + 128) >> 8;
	int b_ = (298 * c + 516 * d + 128) >> 8;

	*r = (uint8_t)(r_ < 0 ? 0 : r_ > 255 ? 255 : r_);
	*g = (uint8_t)(g_ < 0 ? 0 : g_ > 255 ? 255 : g_);
	*b = (uint8_t)(b_ < 0 ? 0 : b_ > 255 ? 255 : b_);
}
void convert_i420_to_bgra(struct obs_source_frame *frame, uint8_t *dst_bgra)
{
	int width = frame->width;
	int height = frame->height;

	uint8_t *Y = frame->data[0];
	uint8_t *U = frame->data[1];
	uint8_t *V = frame->data[2];

	int y_pitch = frame->linesize[0];
	int u_pitch = frame->linesize[1];
	int v_pitch = frame->linesize[2];

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint8_t y_val = Y[y * y_pitch + x];

			// U y V están en 4:2:0 → se muestrean cada 2x2 píxeles
			int u_index = (y / 2) * u_pitch + (x / 2);
			int v_index = (y / 2) * v_pitch + (x / 2);
			uint8_t u_val = U[u_index];
			uint8_t v_val = V[v_index];

			uint8_t r, g, b;
			yuv_to_bgra(y_val, u_val, v_val, &b, &g,
				    &r); // OpenCV quiere BGR

			int dst_index = (y * width + x) * 4;
			dst_bgra[dst_index + 0] = b;
			dst_bgra[dst_index + 1] = g;
			dst_bgra[dst_index + 2] = r;
			dst_bgra[dst_index + 3] = 255;
		}
	}
}
static void convert_nv12_to_bgra(struct obs_source_frame *frame,uint8_t *dst_bgra)
{
	int width = frame->width;
	int height = frame->height;

	uint8_t *Y = frame->data[0];
	uint8_t *UV = frame->data[1];

	int y_pitch = frame->linesize[0];
	int uv_pitch = frame->linesize[1];

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint8_t y_val = Y[y * y_pitch + x];
			uint8_t u_val = UV[(y / 2) * uv_pitch + (x & ~1)];
			uint8_t v_val = UV[(y / 2) * uv_pitch + (x | 1)];

			uint8_t r, g, b;
			yuv_to_bgra(y_val, u_val, v_val, &b, &g, &r);

			int dst_index = (y * width + x) * 4;
			dst_bgra[dst_index + 0] = b;
			dst_bgra[dst_index + 1] = g;
			dst_bgra[dst_index + 2] = r;
			dst_bgra[dst_index + 3] = 255;
		}
	}
}

static void convert_i422_to_bgra(const struct obs_source_frame *frame, uint8_t *dst)
{
	const uint8_t *y_plane = frame->data[0];
	const uint8_t *u_plane = frame->data[1];
	const uint8_t *v_plane = frame->data[2];
	int width = frame->width;
	int height = frame->height;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int Y = y_plane[y * frame->linesize[0] + x];
			int U = u_plane[y * frame->linesize[1] + x];
			int V = v_plane[y * frame->linesize[2] + x];

			int C = Y - 16;
			int D = U - 128;
			int E = V - 128;

			int R = (298 * C + 409 * E + 128) >> 8;
			int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
			int B = (298 * C + 516 * D + 128) >> 8;

			R = R < 0 ? 0 : (R > 255 ? 255 : R);
			G = G < 0 ? 0 : (G > 255 ? 255 : G);
			B = B < 0 ? 0 : (B > 255 ? 255 : B);

			int dst_index = (y * width + x) * 4;
			dst[dst_index + 0] = (uint8_t)B;
			dst[dst_index + 1] = (uint8_t)G;
			dst[dst_index + 2] = (uint8_t)R;
			dst[dst_index + 3] = 255; // Alpha
		}
	}
}
static struct obs_source_frame * cube_filter_filter_video(void *data, struct obs_source_frame *frame)
	{
		/*blog(LOG_INFO, "VIDEO render called");*/
		struct cube_filter_data *filter = data;

		if (!frame) {
			blog(LOG_WARNING, "cube_filter_filter_video: frame es NULL");
			return NULL;
		}

		

		bool detected = false;

		if (frame->format == VIDEO_FORMAT_BGRA) {
			blog(LOG_INFO,"Formato BGRA detectado, procesando directamente");
			

		} else if (frame->format == VIDEO_FORMAT_I420) {
			blog(LOG_INFO, "Formato I420 detectado, convirtiendo a BGRA");

			int image_size = frame->width * frame->height * 4;
			uint8_t *bgra_buffer = bmalloc(image_size);

			convert_i420_to_bgra(frame, bgra_buffer);

			bool detected = process_frame_rgba(
				bgra_buffer, frame->width, frame->height,
				filter->width_screen, filter->height_screen,
				&filter->last_result);

			bfree(bgra_buffer);
		} else if (frame->format == VIDEO_FORMAT_NV12) {
			

			int image_size = frame->width * frame->height * 4;
			uint8_t *bgra_buffer = bmalloc(image_size);

			convert_nv12_to_bgra(frame, bgra_buffer);

		bool detected = process_frame_rgba(
				bgra_buffer, frame->width, frame->height,
				filter->width_screen, filter->height_screen,
				&filter->last_result);

			if (detected && filter->last_result.detected) {
				filter->pos_x = filter->last_result.tvec[0];
				filter->pos_y = filter->last_result.tvec[1];
				filter->pos_z = filter->last_result.tvec[2];

				filter->current_rotation_x_angle =filter->last_result.euler_x;
				filter->current_rotation_y_angle =filter->last_result.euler_y;
				filter->current_rotation_z_angle =filter->last_result.euler_z;

				
			} else {
				filter->last_result.detected = false;
			}
			bfree(bgra_buffer);
		} else if (frame->format == VIDEO_FORMAT_I422) {
			

			int image_size = frame->width * frame->height * 4;
			uint8_t *bgra_buffer = bmalloc(image_size);

			convert_i422_to_bgra(frame, bgra_buffer);

			
			bool detected = process_frame_rgba(
				bgra_buffer, frame->width, frame->height,
				filter->width_screen, filter->height_screen,
				&filter->last_result);

				if (detected && filter->last_result.detected) {
				filter->pos_x = filter->last_result.screen_pos_x;
				filter->pos_y = filter->last_result.screen_pos_y;

			
				// filter->pos_z = filter->last_result.tvec[2]; // Opción 2: Usar la profundidad real del marcador

				// Rotaciones Euler para orientar el modelo 3D
				// euler_x, euler_y, euler_z ya contienen Pitch, Yaw y Roll en grados.
				filter->current_rotation_x_angle = filter->last_result.euler_x; // Pitch (rotación alrededor del eje Y)
				filter->current_rotation_y_angle = filter->last_result.euler_y; // Yaw (rotación alrededor del eje Z)
				filter->current_rotation_z_angle = filter->last_result.euler_z; // Roll (rotación alrededor del eje X)

				blog(LOG_INFO,
					 "Marcador ID %d detectado, pos=(%.2f, %.2f, %.2f), rot=(%.2f, %.2f, %.2f)",
					 filter->last_result.id, filter->pos_x,
					 filter->pos_y, filter->pos_z,
				     filter->current_rotation_x_angle,
				     filter->current_rotation_y_angle,
				     filter->current_rotation_z_angle);
			} else {
				filter->last_result.detected = false;
			}
			bfree(bgra_buffer);
		} else
			{
			blog(LOG_WARNING, "Formato no compatible: %d", frame->format);
		}

		/*if (detected && filter->last_result.detected) {
			blog(LOG_INFO,
				 "SIIIIIIIII Marcador ID %d detectado en posición t=(%.2f, %.2f, %.2f) m",
				 filter->last_result.id, filter->last_result.tvec[0],
				 filter->last_result.tvec[1], filter->last_result.tvec[2]);
					


		} else {
			blog(LOG_INFO, "NOOOOOO se detectó ningún marcador");
		}*/

		return frame;
	}

static uint32_t cube_source_get_width(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->width_screen;
}

static uint32_t cube_source_get_height(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->height_screen;
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

	data->texture = gs_texture_create(data->width_screen, data->height_screen, GS_RGBA, 1, NULL, GS_RENDER_TARGET);
	data->zstencil = gs_zstencil_create(data->width_screen, data->height_screen, GS_Z32F);
	blog(LOG_INFO, "create whiteboard texture %d %d", data->width_screen, data->height_screen);

	obs_leave_graphics();
}

static const char *cube_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "SJ_3D";
}

static void *cube_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct cube_filter_data *data =bzalloc(sizeof(struct cube_filter_data));
	
	data->source = source;
	data->g_meshes = NULL;
	data->g_mesh_count = 0;
	data->model_width = NULL;  // 
	data->model_height = NULL; // 

	initialize_aruco_detector();
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		data->width_screen = ovi.base_width;
		data->height_screen = ovi.base_height;
		create_whiteboard_texture(data);
	} else {
		blog(LOG_WARNING, "Whiteboard: Failed to get video resolution");
	}

	return data;
}

static void cube_filter_destroy(void *data)
{
	/*blog(LOG_WARNING, "CERRANDO");*/
	struct cube_filter_data *filter = (struct cube_filter_data *)data;
	cleanup_global_meshes(&filter->g_meshes, &filter->g_mesh_count, &filter->model_width, &filter->model_height);
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
	/*blog(LOG_INFO, "Filter tick called");*/
	struct cube_filter_data *filter = (struct cube_filter_data *)data;

	 // Capturamos los píxeles justo antes de salir del contexto gráfico:

	obs_enter_graphics();

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
	  uint8_t *final_frame_pixels = NULL;
    uint32_t final_frame_pitch = 0;

  

    obs_leave_graphics();
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
		props, "scale", obs_module_text("Escala"), 0.1f, 1000, 0.01f);
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
		"Modelos 3D (*.obj *.fbx *.dae *.gltf);;Todos los archivos (*.*)",
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

	if (!filter->model_path_str ||
	    strcmp(filter->model_path_str, new_model_path_c_str) != 0 ||
	    filter->g_mesh_count == 0) {
		bfree(filter->model_path_str);
		filter->model_path_str = bstrdup(new_model_path_c_str);

		if (filter->model_path_str &&
		    strlen(filter->model_path_str) > 0) {
			blog(LOG_INFO, "Cargando nuevo modelo desde: %s",
			     filter->model_path_str);
			load_model_c(filter->model_path_str, &filter->g_meshes,&filter->g_mesh_count, &filter->model_width,&filter->model_height);
		}
	}
}


static void cube_filter_tick(void *data, float seconds)
{
	
	struct cube_filter_data *filter = data;
	struct obs_video_info video_info;

	if (obs_get_video_info(&video_info)) {
		if (video_info.base_width != filter->width_screen ||
		    video_info.base_height != filter->height_screen) {
			filter->width_screen = video_info.base_width;
			filter->height_screen = video_info.base_height;
			create_whiteboard_texture(filter);
		}
	}

	blog(LOG_INFO, "TICKTICK");

	obs_enter_graphics();
	gs_render_start(true);
	gs_viewport_push();
	gs_set_viewport(0, 0, filter->width_screen, filter->height_screen);
	gs_projection_push();
	gs_set_3d_mode(60.0f, 0.01f, 5000);
	gs_blend_state_push();
	gs_reset_blend_state();

	gs_enable_depth_test(true);
	gs_depth_function(GS_LESS);

	gs_texture_t *prev_render_target = gs_get_render_target();
	gs_texture_t *prev_zstencil_target = gs_get_zstencil_target();
	gs_set_render_target(filter->texture, filter->zstencil);

	gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH,(float[]){0.0f, 0.0f, 0.0f, 0.0f}, 1.0f, 0);
	gs_matrix_push();
	gs_matrix_identity();
	
	gs_matrix_translate3f(filter->pos_x, filter->pos_y, filter->pos_z);
	
	//gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, filter->current_rotation_z_angle );
	//gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f,filter->current_rotation_y_angle );
	//gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f,filter->current_rotation_x_angle );
	//gs_matrix_scale3f(filter->scale, filter->scale, filter->scale);
	render_model_c(filter->g_meshes, filter->g_mesh_count,filter->model_width,filter->model_height,filter->scale);
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
	.output_flags = OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW ,
	.get_name = cube_filter_get_name,
	.create = cube_filter_create,
	.destroy = cube_filter_destroy,
	.video_render = cube_filter_render,
	.video_tick = cube_filter_tick,
	.get_properties = cube_filter_properties,
	.update = cube_filter_update,
	.save = cube_filter_save,
	.get_defaults = cube_filter_defaults,
	.load=cube_filter_load,
	.filter_video = cube_filter_filter_video,
		
	
};
bool obs_module_load(void)
{
	blog(LOG_INFO, "[CUBE] Registrando filtro");
	obs_register_source(&cube_filter);
	blog(LOG_INFO, "[CUBE] Registrade");
	return true;
}