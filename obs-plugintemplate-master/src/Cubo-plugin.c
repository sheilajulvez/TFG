// Inclusión de las cabeceras del API de OBS Studio.
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <graphics/vec4.h>

// Inclusión de bibliotecas estándar de C.
#include <string.h>
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
	float *model_height;

	struct Mesh *g_meshes;
	size_t g_mesh_count;
	char *model_path_str;
	char *texture_path_str;
	// Parámetros de posición / escala / rotación manual
	float pos_x;
	float pos_y;
	float pos_z;
	float scale;
	float current_scale;
	float rotation_x;
	float rotation_y;
	float rotation_z;
	gs_texture_t *loaded_texture;

	// --- INICIO: NUEVOS OFFSETS PARA AR ---
	float ar_offset_pos_x;
	float ar_offset_pos_y;
	float ar_offset_pos_z;
	float ar_offset_rot_x;
	float ar_offset_rot_y;
	float ar_offset_rot_z;
	// --- FIN: NUEVOS OFFSETS PARA AR ---

	// Resultados del ArUco
	ArucoDetector *detector; //
	ArucoResult last_result; //

	int mode;
};

static struct obs_source_frame *filter_video(void *data,
					     struct obs_source_frame *frame)
{
	struct cube_filter_data *filter = data;

	if (!frame) {
		blog(LOG_WARNING, "cube_filter_filter_video: frame es NULL");
		return NULL;
	}

	if (filter->mode == 0) { // 0 = 3D
		filter->current_scale = filter->scale;
		// Asegurarnos que 'detected' esté en false si no estamos trackeando
		filter->last_result.detected = false;
		// En modo 3D, pos_x/y/z se leen directamente de los sliders (ya cargados en filter_update)
		return frame;
	}

	bool detected = false;

	// Llamamos a process_frame_rgba, que rellena filter->last_result (incluyendo rvec)
	detected = process_frame_rgba(filter->detector, frame,
				      filter->width_screen,
				      filter->height_screen, frame->width,
				      frame->height, &filter->last_result);

	if (detected && filter->last_result.detected) {
		// --- INICIO: APLICAR OFFSETS DE POSICIÓN AR ---
		filter->pos_x = filter->last_result.screen_pos_x +filter->ar_offset_pos_x;
		filter->pos_y = filter->last_result.screen_pos_y +filter->ar_offset_pos_y;
		filter->pos_z =0 +filter->ar_offset_pos_z; // Usamos 0 como base Z del marker + offset
		// --- FIN: APLICAR OFFSETS DE POSICIÓN AR ---

		const float reference_distance = 1.0f;

		if (filter->last_result.tvec[2] > 0.1f) {
			filter->current_scale = filter->scale;
			// * (reference_distance / filter->last_result.tvec[2]);
		} else {
			filter->current_scale = filter->scale;
		}

	} else {
		filter->last_result.detected = false;
	}

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
gs_texture_t *load_texture_file(const char *path)
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

void create_texture(struct cube_filter_data *data)
{
	obs_enter_graphics();

	if (data->texture != NULL) {
		gs_texture_destroy(data->texture);
		data->texture = NULL;
	}

	data->texture = gs_texture_create(data->width_screen,
					  data->height_screen, GS_RGBA, 1, NULL,
					  GS_RENDER_TARGET);
	data->zstencil = gs_zstencil_create(data->width_screen,
					    data->height_screen, GS_Z16);
	blog(LOG_INFO, "create whiteboard texture %d %d", data->width_screen,
	     data->height_screen);

	obs_leave_graphics();
}

static const char *filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "SJ_3D";
}

static void *filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct cube_filter_data *data =
		bzalloc(sizeof(struct cube_filter_data));

	data->source = source;
	data->g_meshes = NULL;
	data->g_mesh_count = 0;
	data->model_width = NULL;  //
	data->model_height = NULL; //
	data->loaded_texture = NULL;

	data->detector =
		initialize_aruco_detector(0.1f, ARUCO_DICT_ORIGINAL, NULL);

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		data->width_screen = ovi.base_width;
		data->height_screen = ovi.base_height;
		create_texture(data);
	} else {
		blog(LOG_WARNING, "Failed to get video resolution");
	}

	return data;
}

static void filter_destroy(void *data)
{
	/*blog(LOG_WARNING, "CERRANDO");*/
	struct cube_filter_data *filter = (struct cube_filter_data *)data;
	cleanup_global_meshes(&filter->g_meshes, &filter->g_mesh_count,
			      &filter->model_width, &filter->model_height,
			      filter->loaded_texture);
	obs_enter_graphics();
	if (filter->texture) {
		gs_texture_destroy(filter->texture);
	}
	if (filter->loaded_texture) {
		gs_texture_destroy(filter->loaded_texture);
	}
	if (filter->zstencil) {
		gs_zstencil_destroy(filter->zstencil);
	}

	obs_leave_graphics();
	bfree(filter->model_path_str);
	bfree(filter->texture_path_str);
	bfree(filter);
}

static void filter_render(void *data, gs_effect_t *effect)
{
	struct cube_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->source);
	if (!target) {
		return;
	}

	uint32_t width = obs_source_get_width(filter->source);
	uint32_t height = obs_source_get_height(filter->source);

	// If there's no model or valid dimensions, just draw the original source
	if (!filter->g_meshes || width == 0 || height == 0) {
		obs_source_skip_video_filter(filter->source);
		return;
	}
	if (filter->mode == 1 && !filter->last_result.detected) { // 0 = 3D
		obs_source_skip_video_filter(filter->source);
		return;
	}
	obs_enter_graphics();
	if (!filter->texture ||
	    gs_texture_get_width(filter->texture) != width ||
	    gs_texture_get_height(filter->texture) != height) {
		if (filter->texture)
			gs_texture_destroy(filter->texture);
		if (filter->zstencil)
			gs_zstencil_destroy(filter->zstencil);

		filter->texture = gs_texture_create(width, height, GS_RGBA, 1,
						    NULL, GS_RENDER_TARGET);
		filter->zstencil = gs_zstencil_create(width, height, GS_Z16);
	}

	// --- Render the 3D model to our texture ---
	gs_texture_t *prev_render_target = gs_get_render_target();
	gs_zstencil_t *prev_zstencil_target = gs_get_zstencil_target();

	gs_set_render_target(filter->texture, filter->zstencil);
	gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH,
		 (struct vec4[]){{0.0f, 0.0f, 0.0f, 0.0f}}, 1.0f, 0);

	gs_projection_push();
	gs_set_3d_mode(60.0f, 0.1f, 5000.0f); // Use a reasonable near/far plane
	gs_enable_depth_test(true);
	gs_depth_function(GS_LESS);

	gs_matrix_push();
	gs_matrix_identity();

	// **Traslación global** (ya incluye el offset si es modo AR, o la pos 3D si es modo 3D)
	gs_matrix_translate3f(filter->pos_x, filter->pos_y, filter->pos_z);

	// --- INICIO: APLICAR ROTACIONES (3D o AR Offset) ---
	if (filter->mode == 0) { // Modo 3D
		// Aplicar rotación manual de los sliders 3D
		gs_matrix_rotaa4f(1, 0, 0, filter->rotation_x);
		gs_matrix_rotaa4f(0, 1, 0, filter->rotation_y);
		gs_matrix_rotaa4f(0, 0, 1, filter->rotation_z);
	
	} 
	// --- FIN: APLICAR ROTACIONES ---

	
	render_model_c(filter->g_meshes, filter->g_mesh_count,
		       filter->model_width, filter->model_height,
		       filter->current_scale, filter->last_result.rvec,
		       filter->last_result.detected);

	gs_matrix_pop();
	gs_projection_pop();
	gs_enable_depth_test(false);

	gs_set_render_target(prev_render_target, prev_zstencil_target);
	obs_leave_graphics();

	// --- Blend the original source and our 3D model texture ---
	obs_enter_graphics();
	// 1. Draw the original video frame
	obs_source_video_render(target);

	// 2. Blend our 3D model on top
	gs_blend_state_push();
	gs_enable_blending(true);
	gs_blend_function(GS_BLEND_SRCALPHA,
			  GS_BLEND_INVSRCALPHA); // Standard alpha blending

	gs_effect_t *base_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	while (gs_effect_loop(base_effect, "Draw")) {
		obs_source_draw(filter->texture, 0, 0, 0, 0, false);
	}

	gs_blend_state_pop();
	obs_leave_graphics();
}
static bool render_mode_changed(obs_properties_t *props,
				obs_property_t *property, obs_data_t *settings)
{
	int mode = (int)obs_data_get_int(settings, "render_mode");
	bool show_3d = (mode == 0);
	bool show_ar = (mode == 1);

	obs_property_set_visible(obs_properties_get(props, "pos_x"), show_3d);
	obs_property_set_visible(obs_properties_get(props, "pos_y"), show_3d);
	obs_property_set_visible(obs_properties_get(props, "pos_z"), show_3d);
	obs_property_set_visible(
		obs_properties_get(props, "rotation_x_slider_value"), show_3d);
	obs_property_set_visible(
		obs_properties_get(props, "rotation_y_slider_value"), show_3d);
	obs_property_set_visible(
		obs_properties_get(props, "rotation_z_slider_value"), show_3d);

	// Propiedades AR
	obs_property_set_visible(obs_properties_get(props, "marker_id"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "marker_size"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "marker_dict"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "calibration_path"),
				 show_ar);

	// --- INICIO: VISIBILIDAD DE NUEVOS OFFSETS AR ---
	obs_property_set_visible(obs_properties_get(props, "ar_offset_pos_x"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_pos_y"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_pos_z"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_rot_x"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_rot_y"),
				 show_ar);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_rot_z"),
				 show_ar);
	// --- FIN: VISIBILIDAD DE NUEVOS OFFSETS AR ---

	return true;
}

static obs_properties_t *filter_properties(void *data)
{

	obs_properties_t *props = obs_properties_create();
	obs_property_t *combo = obs_properties_add_list(props, "render_mode",
							"Modo de renderizado",
							OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(combo, "3D", 0);
	obs_property_list_add_int(combo, "AR", 1);
	obs_property_set_modified_callback(combo, render_mode_changed);

	// Propiedades 3D
	obs_properties_add_float_slider(props, "pos_x", "Posición X", -3000.0f,
					3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "pos_y", "Posición Y", -3000.0f,
					3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "pos_z", "Posición Z", -3000.0f,
					3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "scale", "Escala", 0.1f, 1000.0f,
					0.01f);
	obs_properties_add_float_slider(props, "rotation_x_slider_value",
					"Rotación X (Grados)", -360.0f, 360.0f,
					1.0f);
	obs_properties_add_float_slider(props, "rotation_y_slider_value",
					"Rotación Y (Grados)", -360.0f, 360.0f,
					1.0f);
	obs_properties_add_float_slider(props, "rotation_z_slider_value",
					"Rotación Z (Grados)", -360.0f, 360.0f,
					1.0f);

	// Propiedades Comunes
	obs_properties_add_path(
		props, "texture_path", "Ruta de la Textura", OBS_PATH_FILE,
		"Imágenes (*.png *.jpg *.jpeg *.bmp *.tga);;Todos (*.*)", NULL);
	obs_properties_add_path(
		props, "model_path", "Ruta del Modelo 3D", OBS_PATH_FILE,
		"Modelos 3D (*.obj *.fbx *.dae *.gltf);;Todos (*.*)", NULL);

	// Propiedades AR
	obs_properties_add_int(props, "marker_id", "ID del Marker", 0, 100, 1);
	obs_properties_add_float_slider(props, "marker_size",
					"Tamaño del Marker", 0.1f, 10.0f, 0.1f);
	obs_property_t *dict = obs_properties_add_list(props, "marker_dict",
						       "Diccionario de Marker",
						       OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(dict, "Original", ARUCO_DICT_ORIGINAL);
	obs_property_list_add_int(dict, "4×4 (100)", ARUCO_DICT_4X4_100);
	obs_property_list_add_int(dict, "5×5 (100)", ARUCO_DICT_5X5_100);
	obs_property_list_add_int(dict, "6×6 (100)", ARUCO_DICT_6X6_100);
	obs_property_list_add_int(dict, "7×7 (100)", ARUCO_DICT_7X7_100);
	obs_property_list_add_int(dict, "MIP Original", ARUCO_DICT_MIP_ORIGINAL);
	obs_properties_add_path(props, "calibration_path",
				"Archivo de Calibración", OBS_PATH_FILE,
				"YAML (*.yml *.yaml);;Todos (*.*)", NULL);

	// --- INICIO: NUEVOS SLIDERS OFFSET AR ---
	obs_properties_add_float_slider(props, "ar_offset_pos_x",
					"AR Offset Posición X", -1000,
					1000.0f, 10.0f);
	obs_properties_add_float_slider(props, "ar_offset_pos_y",
					"AR Offset Posición Y", -1000.0f,
					1000.0f, 10.0f);
	obs_properties_add_float_slider(props, "ar_offset_pos_z",
					"AR Offset Posición Z", -1000.0f,
					1000.0f, 10.0f);
	obs_properties_add_float_slider(props, "ar_offset_rot_x",
					"AR Offset Rotación X", -360.0f, 360.0f,
					1.0f);
	obs_properties_add_float_slider(props, "ar_offset_rot_y",
					"AR Offset Rotación Y", -360.0f, 360.0f,
					1.0f);
	obs_properties_add_float_slider(props, "ar_offset_rot_z",
					"AR Offset Rotación Z", -360.0f, 360.0f,
					1.0f);
	// --- FIN: NUEVOS SLIDERS OFFSET AR ---

	obs_data_t *temp_settings = obs_data_create();
	render_mode_changed(props, combo, temp_settings);
	obs_data_release(temp_settings);
	return props;
}

static void filter_update(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	if (!filter || !settings)
		return;

	/* Valores básicos */
	filter->mode = (int)obs_data_get_int(settings, "render_mode");
	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
	filter->scale = (float)obs_data_get_double(settings, "scale");
	filter->rotation_x =
		(float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y =
		(float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z =
		(float)obs_data_get_double(settings, "rotation_z_slider_value");

	// --- INICIO: LEER VALORES OFFSET AR ---
	filter->ar_offset_pos_x =
		(float)obs_data_get_double(settings, "ar_offset_pos_x");
	filter->ar_offset_pos_y =
		(float)obs_data_get_double(settings, "ar_offset_pos_y");
	filter->ar_offset_pos_z =
		(float)obs_data_get_double(settings, "ar_offset_pos_z");
	filter->ar_offset_rot_x =
		(float)obs_data_get_double(settings, "ar_offset_rot_x");
	filter->ar_offset_rot_y =
		(float)obs_data_get_double(settings, "ar_offset_rot_y");
	filter->ar_offset_rot_z =
		(float)obs_data_get_double(settings, "ar_offset_rot_z");
	// --- FIN: LEER VALORES OFFSET AR ---

	int id = (int)obs_data_get_int(settings, "marker_id");
	float size = (float)obs_data_get_double(settings, "marker_size");

	if (filter->detector) {
		if (size != get_marker_size(filter->detector))
			set_marker_size(filter->detector, size);
		if (id != get_marker_id(filter->detector))
			set_marker_id(filter->detector, id);
	}

	/* --------------------------
        Modelo: carga si la ruta cambia o no hay mallas aún
        -------------------------- */
	const char *new_model_path_c_str =
		obs_data_get_string(settings, "model_path");
	if (!new_model_path_c_str)
		new_model_path_c_str = "";

	if (!filter->model_path_str ||
	    strcmp(filter->model_path_str, new_model_path_c_str) != 0 ||
	    filter->g_mesh_count == 0) {

		bfree(filter->model_path_str);
		filter->model_path_str = bstrdup(new_model_path_c_str);

		if (filter->model_path_str && filter->model_path_str[0]) {
			blog(LOG_INFO, "[CUBE] Cargando nuevo modelo desde: %s",
			     filter->model_path_str);
			load_model_c(filter->model_path_str, &filter->g_meshes,
				     &filter->g_mesh_count,
				     &filter->model_width,
				     &filter->model_height);
		}
	}

	/* --------------------------
        Textura: lógica de recarga robusta, SIN CAMBIAR cómo se destruyen las texturas
        -------------------------- */
	const char *new_texture_path_c_str =
		obs_data_get_string(settings, "texture_path");
	if (!new_texture_path_c_str)
		new_texture_path_c_str = "";

	bool model_present =
		(filter->g_meshes != NULL && filter->g_mesh_count > 0);

	bool texture_path_changed = false;
	if (!filter->texture_path_str)
		texture_path_changed = (new_texture_path_c_str[0] != '\0');
	else
		texture_path_changed = (strcmp(filter->texture_path_str,
					       new_texture_path_c_str) != 0);

	/* Forzar recarga si:
        - la ruta cambió (texture_path_changed)
        - o la ruta es igual pero no hay loaded_texture en memoria (p. ej. tras reinicio de OBS)
        - y además hay mallas cargadas (model_present)
     */
	bool should_reload_texture = false;
	if (model_present) {
		if (texture_path_changed)
			should_reload_texture = true;
		else if (filter->loaded_texture == NULL &&
			 new_texture_path_c_str[0] != '\0')
			should_reload_texture = true;
	}

	if (should_reload_texture) {
		/* Mantengo exactamente la secuencia original para borrar/recargar */
		bfree(filter->texture_path_str);
		filter->texture_path_str = bstrdup(new_texture_path_c_str);

		/* Si existía una textura cargada, la destruyo (igual que en tu lógica original). */
		if (filter->loaded_texture) {
			obs_enter_graphics();
			gs_texture_destroy(filter->loaded_texture);
			filter->loaded_texture = NULL;
			obs_leave_graphics();
		}

		if (filter->texture_path_str && filter->texture_path_str[0]) {
			filter->loaded_texture =
				load_texture_file(filter->texture_path_str);
			if (filter->loaded_texture) {
				blog(LOG_INFO,
				     "[CUBE] Nueva textura cargada desde: %s",
				     filter->texture_path_str);
				replace_mesh_textures(filter->g_meshes,
						      filter->g_mesh_count,
						      filter->loaded_texture,
						      NULL);
			} else {
				blog(LOG_WARNING,
				     "[CUBE] No se pudo cargar la nueva textura desde: %s",
				     filter->texture_path_str);
			}
		} else {
			/* ruta vacía: eliminar texturas en mallas */
			blog(LOG_INFO,
			     "[CUBE] Ruta de textura vacía: eliminando textura en mallas");
			replace_mesh_textures(filter->g_meshes,
					      filter->g_mesh_count, NULL, NULL);
		}
	} else {
		/* Si no toca recargar pero la ruta es vacía y hay textura aplicada, mantenemos comportamiento previo:
           si la ruta está vacía y hay loaded_texture, eliminarla y desasignar de mallas. */
		if (model_present && new_texture_path_c_str[0] == '\0' &&
		    filter->loaded_texture) {
			obs_enter_graphics();
			gs_texture_destroy(filter->loaded_texture);
			filter->loaded_texture = NULL;
			obs_leave_graphics();
			replace_mesh_textures(filter->g_meshes,
					      filter->g_mesh_count, NULL, NULL);
		}
	}

	/* Diccionario del marcador */
	int marker_dict = obs_data_get_int(settings, "marker_dict");
	if (filter->detector &&
	    marker_dict != get_marker_dictionary(filter->detector)) {
		set_marker_dictionary(filter->detector, marker_dict);
	}
}

static void filter_tick(void *data, float seconds)
{

	struct cube_filter_data *filter = data;
	struct obs_video_info video_info;

	if (obs_get_video_info(&video_info)) {
		if (video_info.base_width != filter->width_screen ||
		    video_info.base_height != filter->height_screen) {
			filter->width_screen = video_info.base_width;
			filter->height_screen = video_info.base_height;
			create_texture(filter);
		}
	}
}

static void filter_save(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	blog(LOG_INFO, "[CUBE] guardando valores");
	obs_data_set_double(settings, "pos_x", filter->pos_x);
	obs_data_set_double(settings, "pos_y", filter->pos_y);
	obs_data_set_double(settings, "pos_z", filter->pos_z);
	obs_data_set_double(settings, "scale", filter->scale);
	obs_data_set_double(settings, "rotation_x_slider_value",
			    filter->rotation_x);
	obs_data_set_double(settings, "rotation_y_slider_value",
			    filter->rotation_y);
	obs_data_set_double(settings, "rotation_z_slider_value",
			    filter->rotation_z);
	obs_data_set_double(settings, "marker_size",
			    get_marker_size(filter->detector));
	obs_data_set_int(settings, "marker_id",
			 get_marker_id(filter->detector));
	obs_data_set_int(settings, "marker_dict",
			 get_marker_dictionary(filter->detector));

	// --- INICIO: GUARDAR OFFSETS AR ---
	obs_data_set_double(settings, "ar_offset_pos_x",
			    filter->ar_offset_pos_x);
	obs_data_set_double(settings, "ar_offset_pos_y",
			    filter->ar_offset_pos_y);
	obs_data_set_double(settings, "ar_offset_pos_z",
			    filter->ar_offset_pos_z);
	obs_data_set_double(settings, "ar_offset_rot_x",
			    filter->ar_offset_rot_x);
	obs_data_set_double(settings, "ar_offset_rot_y",
			    filter->ar_offset_rot_y);
	obs_data_set_double(settings, "ar_offset_rot_z",
			    filter->ar_offset_rot_z);
	// --- FIN: GUARDAR OFFSETS AR ---

	if (filter->model_path_str)
		obs_data_set_string(settings, "model_path",
				    filter->model_path_str);
	if (filter->texture_path_str)
		obs_data_set_string(settings, "texture_path",
				    filter->texture_path_str);
	if (get_calibration_path(filter->detector))
		obs_data_set_string(settings, "calibration_path",
				    get_calibration_path(filter->detector));
}

static void filter_load(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	blog(LOG_INFO, "[CUBE] cargando valores");
	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
	filter->scale = (float)obs_data_get_double(settings, "scale");
	filter->rotation_x =(float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y =
		(float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z =
		(float)obs_data_get_double(settings, "rotation_z_slider_value");

	// --- INICIO: CARGAR OFFSETS AR ---
	filter->ar_offset_pos_x =
		(float)obs_data_get_double(settings, "ar_offset_pos_x");
	filter->ar_offset_pos_y =
		(float)obs_data_get_double(settings, "ar_offset_pos_y");
	filter->ar_offset_pos_z =
		(float)obs_data_get_double(settings, "ar_offset_pos_z");
	filter->ar_offset_rot_x =
		(float)obs_data_get_double(settings, "ar_offset_rot_x");
	filter->ar_offset_rot_y =
		(float)obs_data_get_double(settings, "ar_offset_rot_y");
	filter->ar_offset_rot_z =
		(float)obs_data_get_double(settings, "ar_offset_rot_z");
	// --- FIN: CARGAR OFFSETS AR ---

	int id = (int)obs_data_get_int(settings, "marker_id");
	int dict = (int)obs_data_get_int(settings, "marker_dict");
	float siz = (float)obs_data_get_double(settings, "marker_size");
	if (id != get_marker_id(filter->detector))set_marker_id(filter->detector, id);
	if (dict != get_marker_dictionary(filter->detector))set_marker_dictionary(filter->detector, dict);
	if (siz != get_marker_size(filter->detector))set_marker_size(filter->detector, siz);
	const char *mp = obs_data_get_string(settings, "model_path");
	if (filter->model_path_str)bfree(filter->model_path_str);
	filter->model_path_str = (mp && *mp) ? bstrdup(mp) : NULL;
	const char *tp = obs_data_get_string(settings, "texture_path");
	if (filter->texture_path_str)bfree(filter->texture_path_str);
	filter->texture_path_str = (tp && *tp) ? bstrdup(tp) : NULL;
	const char *cp = obs_data_get_string(settings, "calibration_path");
	if (cp && *cp)set_calibration_path(filter->detector, cp);
	blog(LOG_WARNING,"[CUBE] filter_load: fallo al aplicar calibración desde %s", cp);
	filter_update(data, settings);
}
static void filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "pos_x", 0.0);
	obs_data_set_default_double(settings, "pos_y", 0.0);
	obs_data_set_default_double(settings, "pos_z", 0.0);
	obs_data_set_default_double(settings, "scale", 100.0);
	obs_data_set_default_double(settings, "rotation_x_slider_value", 0.0);
	obs_data_set_default_double(settings, "rotation_y_slider_value", 0.0);
	obs_data_set_default_double(settings, "rotation_z_slider_value", 0.0);
	obs_data_set_default_double(settings, "marker_size", 0.1);
	obs_data_set_default_int(settings, "marker_id", 0);
	obs_data_set_default_string(settings, "model_path", "");
	obs_data_set_default_string(settings, "texture_path", "");
	obs_data_set_default_int(settings, "marker_dict", ARUCO_DICT_ORIGINAL);
	obs_data_set_default_string(settings, "calibration_path", "calibration.yml");

	// --- INICIO: DEFAULTS OFFSETS AR ---
	obs_data_set_default_double(settings, "ar_offset_pos_x", 0.0);
	obs_data_set_default_double(settings, "ar_offset_pos_y", 0.0);
	obs_data_set_default_double(settings, "ar_offset_pos_z", 0.0);
	obs_data_set_default_double(settings, "ar_offset_rot_x", 0.0);
	obs_data_set_default_double(settings, "ar_offset_rot_y", 0.0);
	obs_data_set_default_double(settings, "ar_offset_rot_z", 0.0);
	// --- FIN: DEFAULTS OFFSETS AR ---
}
static struct obs_source_info cube_filter = {
	.id = "cube_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = filter_get_name,
	.create = filter_create,
	.destroy = filter_destroy,
	.video_render = filter_render,
	.video_tick = filter_tick,
	.get_properties = filter_properties,
	.update = filter_update,
	.save = filter_save,
	.get_defaults = filter_defaults,
	.load = filter_load,
	.filter_video = filter_video,

};
bool obs_module_load(void)
{
	blog(LOG_INFO, "[CUBE] Registrando filtro");
	obs_register_source(&cube_filter);
	blog(LOG_INFO, "[CUBE] Registrade");
	return true;
}