

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

	#include "yuv2bgra.h"
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
		float rotation_x;
		float rotation_y;
		float rotation_z;
		gs_texture_t *loaded_texture;

		// Resultados del ArUco
		ArucoDetector *detector; // 
		ArucoResult last_result; //
	};


	static struct obs_source_frame *filter_video(void *data, struct obs_source_frame *frame)
	{
		struct cube_filter_data *filter = data;

		if (!frame) {
			blog(LOG_WARNING, "cube_filter_filter_video: frame es NULL");
			return NULL;
		}

		bool detected = false;
		uint8_t *bgra_buffer = NULL;
		int image_size = frame->width * frame->height * 4;
		get_uv_func get_uv = NULL;
		blog(LOG_WARNING, "FILTER VIDEO");
		switch (frame->format) {
		case VIDEO_FORMAT_BGRA:
			blog(LOG_INFO,"Formato BGRA detectado, procesando directamente");

			detected = process_frame_rgba(filter->detector,frame->data[0], frame->width,frame->height, filter->width_screen, filter->height_screen, &filter->last_result);
			break;

		case VIDEO_FORMAT_I420:
			blog(LOG_INFO, "Formato I420 detectado, convirtiendo a BGRA");
			get_uv = get_uv_i420;
			break;

		case VIDEO_FORMAT_NV12:
			blog(LOG_INFO, "Formato NV12 detectado, convirtiendo a BGRA");
			get_uv = get_uv_nv12;
			break;

		case VIDEO_FORMAT_I422:
			//blog(LOG_INFO, "Formato I422 detectado, convirtiendo a BGRA");
			get_uv = get_uv_i422;
			break;

		default:
			blog(LOG_WARNING, "Formato no compatible: %d", frame->format);
			break;
		}

		if (get_uv) {
			bgra_buffer = bmalloc(image_size);
			convert_yuv_to_bgra_generic(frame, bgra_buffer, get_uv);

			detected = process_frame_rgba(filter->detector,bgra_buffer, frame->width,frame->height,filter->width_screen,  filter->height_screen, &filter->last_result);

			bfree(bgra_buffer);
		}

		if (detected && filter->last_result.detected) {
			filter->pos_x = filter->last_result.screen_pos_x;
			filter->pos_y = filter->last_result.screen_pos_y;
			filter->pos_z = 0;

			filter->rotation_x = filter->last_result.euler_x;
			filter->rotation_y = filter->last_result.euler_y;
			filter->rotation_z = filter->last_result.euler_z;
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
			gs_image_file_free(&image); // Libera los datos internos de la imagen, no la textura
			return new_texture;
		} else {
			if (image.texture) { // Si hubo un intento de crear la textura pero falló la carga
				gs_texture_destroy(image.texture);
			}
			blog(LOG_WARNING, "No se pudo cargar la textura de usuario: %s",path);
			gs_image_file_free(&image); // Asegúrate de liberar la estructura de imagen
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

		data->texture = gs_texture_create(data->width_screen, data->height_screen, GS_RGBA, 1, NULL, GS_RENDER_TARGET);
		data->zstencil = gs_zstencil_create(data->width_screen, data->height_screen, GS_Z16);
		blog(LOG_INFO, "create whiteboard texture %d %d", data->width_screen, data->height_screen);

		obs_leave_graphics();
	}

	static const char *filter_get_name(void *unused)
	{
		UNUSED_PARAMETER(unused);
		return "SJ_3D";
	}

	static void *filter_create(obs_data_t *settings, obs_source_t *source)
	{
		struct cube_filter_data *data =bzalloc(sizeof(struct cube_filter_data));
	
		data->source = source;
		data->g_meshes = NULL;
		data->g_mesh_count = 0;
		data->model_width = NULL;  // 
		data->model_height = NULL; // 
		data->loaded_texture = NULL;
	
		data->detector = initialize_aruco_detector(0.1f, ARUCO_DICT_ORIGINAL, NULL);


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
		cleanup_global_meshes(&filter->g_meshes, &filter->g_mesh_count, &filter->model_width, &filter->model_height, filter->loaded_texture);
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
		//obs_enter_graphics();
		//struct obs_source_frame *frame = obs_source_get_frame(filter->source);
		//if (frame) {
		//
		//
		//obs_source_release_frame(filter->source, frame);
		//}
		//else{
		//	blog(LOG_WARNING, "MMMAMAMAMA");
		//}
		//obs_leave_graphics();
		// Get target dimensions
		uint32_t width = obs_source_get_width(filter->source);
		uint32_t height = obs_source_get_height(filter->source);

		// If there's no model or valid dimensions, just draw the original source
		if (!filter->g_meshes || width == 0 || height == 0) {
			obs_source_skip_video_filter(filter->source);
			return;
		}

		obs_enter_graphics();
		if (!filter->texture ||
			gs_texture_get_width(filter->texture) != width ||
			gs_texture_get_height(filter->texture) != height) {
			if (filter->texture)gs_texture_destroy(filter->texture);
			if (filter->zstencil)gs_zstencil_destroy(filter->zstencil);

			filter->texture = gs_texture_create(width, height, GS_RGBA, 1, NULL, GS_RENDER_TARGET);
			filter->zstencil = gs_zstencil_create(width, height, GS_Z16);
		}

		// --- Render the 3D model to our texture ---
		gs_texture_t *prev_render_target = gs_get_render_target();
		gs_zstencil_t *prev_zstencil_target = gs_get_zstencil_target();

		gs_set_render_target(filter->texture, filter->zstencil);
		gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH, (struct vec4[]){{0.0f, 0.0f, 0.0f, 0.0f}}, 1.0f, 0);

		gs_projection_push();
		gs_set_3d_mode(60.0f, 0.1f, 5000.0f); // Use a reasonable near/far plane
		gs_enable_depth_test(true);
		gs_depth_function(GS_LESS);

		gs_matrix_push();
		gs_matrix_identity();

		// **Traslación global** al marker
		gs_matrix_translate3f(filter->pos_x, filter->pos_y, filter->pos_z);

		// Ahora llamas a render_model_c, que aplicará sus propias rotaciones/centros
		render_model_c(filter->g_meshes, filter->g_mesh_count, filter->model_width, filter->model_height, filter->scale,filter->rotation_x, filter->rotation_y,  filter->rotation_z);

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
		gs_blend_function(GS_BLEND_SRCALPHA,GS_BLEND_INVSRCALPHA); // Standard alpha blending

		gs_effect_t *base_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		while (gs_effect_loop(base_effect, "Draw")) {
			obs_source_draw(filter->texture, 0, 0, 0, 0, false);
		}

		gs_blend_state_pop();
		obs_leave_graphics();
	}


	static obs_properties_t *filter_properties(void *data)
	{
		obs_properties_t *props = obs_properties_create();
		obs_properties_add_group(props, "position_group",obs_module_text("Posición"), OBS_GROUP_NORMAL,props);
		obs_properties_add_float_slider(props, "pos_x",obs_module_text("Posición X"), -3000.0f,3000.0f, 10.0f);
		obs_properties_add_float_slider(props, "pos_y",obs_module_text("Posición Y"), -3000.0f,3000.0f, 10.0f);
		obs_properties_add_float_slider(props, "pos_z",obs_module_text("Posición Z"), -3000.0f,3000.0f, 10.0f);
		obs_properties_add_float_slider(props, "scale", obs_module_text("Escala"), 0.1f, 1000, 0.01f);
		obs_properties_add_float_slider(props, "rotation_z_slider_value",obs_module_text("Rotación Z (Grados)"),-360.0f, 360.0f, 1.0f);
		obs_properties_add_float_slider(props, "rotation_x_slider_value",obs_module_text("Rotación Y (Grados)"),-360.0f, 360.0f, 1.0f);
		obs_properties_add_float_slider(props, "rotation_y_slider_value",obs_module_text("Rotación X (Grados)"),-360.0f, 360.0f, 1.0f);
		obs_properties_add_path(props, "model_path", obs_module_text("Ruta del Modelo 3D"),OBS_PATH_FILE,"Modelos 3D (*.obj *.fbx *.dae *.gltf);;Todos los archivos (*.*)",NULL);
		obs_properties_add_int(props, "marker_id", "ID del Marker", 0, 100, 1);
		obs_properties_add_float_slider(props, "marker_size",obs_module_text("maker_size "),0.1f,10.f, 1.0f);
		obs_properties_add_path( props, "texture_path", obs_module_text("Ruta de la Textura"), OBS_PATH_FILE,"Archivos de Imagen (*.png *.jpg *.jpeg *.bmp *.tga);;Todos los archivos (*.*)", NULL);
		obs_property_t *p = obs_properties_add_list(
			props, "marker_dict",
			obs_module_text("Marker Dictionary"),
			OBS_COMBO_TYPE_LIST,
			OBS_COMBO_FORMAT_INT
		);

		obs_property_list_add_int(p, "Original",    0);
		obs_property_list_add_int(p, "4×4 (100)",  ARUCO_DICT_4X4_100);
		obs_property_list_add_int(p, "5×5 (100)",   ARUCO_DICT_5X5_100);
		obs_property_list_add_int(p, "6×6 (100)",  ARUCO_DICT_6X6_100);
		obs_property_list_add_int(p, "7×7 (100)",  ARUCO_DICT_7X7_100);
		obs_property_list_add_int(p, "Original", ARUCO_DICT_ORIGINAL);
		obs_property_list_add_int(p, "MIP", ARUCO_DICT_MIP_ORIGINAL);

		obs_properties_add_path(props, "calibration_path", "Camera Calibration File",OBS_PATH_FILE,"YAML Files (*.yml *.yaml);;All Files (*.*)", NULL);
		return props;
	}

	static void filter_update(void *data, obs_data_t *settings)
	{
		struct cube_filter_data *filter = data;
	
		filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
		filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
		filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
		filter->scale = (float)obs_data_get_double(settings, "scale");
		filter->rotation_x =(float)obs_data_get_double(settings, "rotation_x_slider_value");
		filter->rotation_y =(float)obs_data_get_double(settings, "rotation_y_slider_value");
		filter->rotation_z =(float)obs_data_get_double(settings, "rotation_z_slider_value");
		int id=(int)obs_data_get_double(settings, "marker_id");
		int size = (float)obs_data_get_double(settings, "marker_size");
		if (size != get_marker_size(filter->detector)) {
		
			set_marker_size(filter->detector, size);
		}
		if (id != get_marker_id(filter->detector))
		{
	
			set_marker_id(filter->detector, id);
		}
		const char *new_model_path_c_str =obs_data_get_string(settings, "model_path");
		if (!filter->model_path_str || strcmp(filter->model_path_str, new_model_path_c_str) != 0 ||filter->g_mesh_count == 0) {
			bfree(filter->model_path_str);
			filter->model_path_str = bstrdup(new_model_path_c_str);

			if (filter->model_path_str && strlen(filter->model_path_str) > 0) {
				blog(LOG_INFO, "Cargando nuevo modelo desde: %s",filter->model_path_str);
				load_model_c(filter->model_path_str, &filter->g_meshes,&filter->g_mesh_count, &filter->model_width,&filter->model_height);
			}
		}

		const char *new_texture_path_c_str = obs_data_get_string(settings, "texture_path"); 
		if (!filter->texture_path_str || strcmp(filter->texture_path_str, new_texture_path_c_str) != 0 &&filter->g_meshes ) {

			// 1. Liberar la ruta de la textura ALMACENADA ANTERIORMENTE
			bfree(filter->texture_path_str);
			// 2. Duplicar la NUEVA ruta para guardarla en el filtro
			filter->texture_path_str = bstrdup(new_texture_path_c_str);

			// 3 Destruir la TEXTURA ANTERIOR cargada en 'filter->loaded_texture'
			// Esto evita la fuga de memoria al cargar una nueva.
			if (filter->loaded_texture) {
				obs_enter_graphics(); // Entrar al contexto de gráficos para operar con texturas
				gs_texture_destroy(filter->loaded_texture);
				filter->loaded_texture =NULL; // Importante: poner a NULL después de destruir para evitar punteros colgantes
				obs_leave_graphics(); // Salir del contexto de gráficos
			}

			// 4. Cargar la NUEVA textura si la ruta no está vacía
			if (filter->texture_path_str &&strlen(filter->texture_path_str) > 0) {
				// Asumo que 'load_texture_file' es tu función para cargar gs_texture_t* desde una ruta
				filter->loaded_texture =load_texture_file(filter->texture_path_str);
				if (filter->loaded_texture) {
					blog(LOG_INFO, "Nueva textura cargada desde: %s", filter->texture_path_str);
					apply_texture_to_all_meshes(filter->g_meshes, filter->g_mesh_count,filter->loaded_texture);
				} else {
					blog(LOG_WARNING, "No se pudo cargar la nueva textura desde: %s",filter->texture_path_str);
				}
			} else {
				// Si la ruta está vacía, no hay textura para cargar (filter->loaded_texture ya es NULL)
				blog(LOG_INFO,"Ruta de textura vacía. Se eliminará la textura de las mallas.");
			}
		}
		int marker_dict= obs_data_get_int(settings, "marker_dict");
		if (marker_dict != get_marker_dictionary(filter->detector)) {
		
			set_marker_dictionary(filter->detector, marker_dict);
		}
		/*const char *path =obs_data_get_string(settings, "calibration_path");
		if(path)set_calibration_path(filter->detector, path);*/
		

	}


	static void filter_tick(void *data, float seconds)
	{
	
		struct cube_filter_data *filter = data;
		struct obs_video_info video_info;

		if (obs_get_video_info(&video_info)) {
			if (video_info.base_width != filter->width_screen ||video_info.base_height != filter->height_screen) {
				filter->width_screen = video_info.base_width;
				filter->height_screen = video_info.base_height;
				create_texture(filter);
			}
		}



	}



	static void filter_save(void *data, obs_data_t *settings)
	{
		struct cube_filter_data *filter = data;
		blog(LOG_INFO, "[CUBE] Guardando valores");
		obs_data_set_double(settings, "pos_x", filter->pos_x);
		obs_data_set_double(settings, "pos_y", filter->pos_y);
		obs_data_set_double(settings, "pos_z", filter->pos_z);
		obs_data_set_double(settings, "scale", filter->scale);
		obs_data_set_double(settings, "rotation_x_slider_value",filter->rotation_x);
		obs_data_set_double(settings, "rotation_y_slider_value", filter->rotation_y);
		obs_data_set_double(settings, "rotation_z_slider_value", filter->rotation_z);
		obs_data_set_double(settings, "marker_size", get_marker_size(filter->detector));
		obs_data_set_int(settings, "marker_id", get_marker_id(filter->detector));
		obs_data_set_int(settings, "marker_dict", get_marker_dictionary(filter->detector));
		if (filter->model_path_str != NULL)obs_data_set_string(settings, "model_path",  filter->model_path_str);
		if (filter->texture_path_str != NULL)obs_data_set_string(settings, "texture_path",  filter->texture_path_str);
		
		if (get_calibration_path(filter->detector))
			obs_data_set_string(settings, "calibration_path",get_calibration_path(filter->detector));

	}
	void filter_load(void *data, obs_data_t *settings)
	{
		struct cube_filter_data *filter = data;
		blog(LOG_INFO, "[CUBE] Cargando valores");

		filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
		filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
		filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
		filter->scale = (float)obs_data_get_double(settings, "scale");

		filter->rotation_x =(float)obs_data_get_double(settings, "rotation_x_slider_value");
		filter->rotation_y =(float)obs_data_get_double(settings, "rotation_y_slider_value");
		filter->rotation_z =(float)obs_data_get_double(settings, "rotation_z_slider_value");

		int id=(int)obs_data_get_int(settings, "marker_id");
		if (id != get_marker_id(filter->detector)) {
		
			set_marker_id(filter->detector, id);
	
		}
		int marker_dict = obs_data_get_int(settings, "marker_dict");
		if (marker_dict != get_marker_dictionary(filter->detector)) {

			set_marker_dictionary(filter->detector, marker_dict);
		}
		int size=(float)obs_data_get_double(settings, "marker_size");
		if (size != get_marker_size(filter->detector)) {
			set_marker_size(filter->detector, size);
		}
		const char *model_path = obs_data_get_string(settings, "model_path");
		if (model_path && *model_path != '\0') {
			filter->model_path_str = bstrdup(model_path);
		}
		const char *texture_path = obs_data_get_string(settings, "texture_path");
		if (texture_path && *texture_path != '\0') {
			filter->model_path_str = bstrdup(model_path);
		}
		const char *path = obs_data_get_string(settings, "calibration_path");

		set_calibration_path(filter->detector, path);

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

	}
	static struct obs_source_info cube_filter = {
		.id = "cube_filter",
		.type = OBS_SOURCE_TYPE_FILTER,
		.output_flags = OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW ,
		.get_name = filter_get_name,
		.create = filter_create,
		.destroy = filter_destroy,
		.video_render = filter_render,
		.video_tick = filter_tick,
		.get_properties = filter_properties,
		.update = filter_update,
		.save = filter_save,
		.get_defaults = filter_defaults,
		.load=filter_load,
		.filter_video = filter_video,
		
	
	};
	bool obs_module_load(void)
	{
		blog(LOG_INFO, "[CUBE] Registrando filtro");
		obs_register_source(&cube_filter);
		blog(LOG_INFO, "[CUBE] Registrade");
		return true;
	}