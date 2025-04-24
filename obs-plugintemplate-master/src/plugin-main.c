//#include <obs.h>
//#include <graphics/graphics.h> // Necesario para los gráficos de OBS
//#include <obs-module.h>
//
//OBS_DECLARE_MODULE()
//// Estructura para el filtro
//typedef struct color_filter {
//	gs_effect_t *effect; // Efecto gráfico
//} color_filter_t;
//
//// Función para renderizar el filtro
//static void *color_filter_create(obs_data_t *settings, obs_source_t *source)
//{
//	color_filter_t *filter =
//		(color_filter_t *)bzalloc(sizeof(color_filter_t));
//
//	// Crear un shader que simplemente pinta todo de rojo
//	filter->effect = gs_effect_create_from_file("red_color_effect.fx",
//						    NULL); // Archivo de efecto
//
//	return filter;
//}
//
//// Función para obtener el nombre del filtro
//static const char *color_filter_get_name(void *data)
//{
//	return "Red Color Filter"; // Nombre del filtro
//}
//
//
//// Función para destruir el filtro
//static void color_filter_destroy(void *data)
//{
//	color_filter_t *filter = (color_filter_t *)data;
//	if (filter) {
//		if (filter->effect) {
//
//			gs_effect_destroy(
//				filter->effect); // Libera el efecto gráfico
//		}
//		bfree(filter); // Libera la memoria del filtro
//	}
//}
//
//
//// Función para obtener las dimensiones del filtro
//static uint32_t color_filter_get_width(void *data)
//{
//	return 1920; // Ancho del filtro
//}
//
//static uint32_t color_filter_get_height(void *data)
//{
//	return 1080; // Alto del filtro
//}
//
//// Información sobre el filtro que OBS utilizará
//static obs_source_info color_filter_info = {
//	"RedColorFilter",        // ID del filtro
//	OBS_SOURCE_TYPE_FILTER,  // Tipo de fuente (filtro)
//	OBS_SOURCE_VIDEO,        // Datos de salida (video)
//	color_filter_get_name,   // Función para obtener el nombre del filtro
//	color_filter_create,     // Función para crear el filtro
//	color_filter_destroy,    // Función para destruir el filtro
//	color_filter_get_width,  // Función para obtener el ancho
//	color_filter_get_height, // Función para obtener el alto
//	      // Función para renderizar el filtro
//};
//
//// Función para renderizar el filtro
//static void color_filter_render(void *data, gs_effect_t *effect)
//{
//	color_filter_t *filter = (color_filter_t *)data;
//	if (filter && filter->effect) {
//		gs_effect_set_texture(gs_effect_get_param_by_name(
//					      filter->effect, "color_texture"),
//				      NULL); // Sin textura
//		gs_effect_set_int(gs_effect_get_param_by_name(filter->effect,
//							      "is_red"),
//				  1); // Parámetro para activar el color rojo
//		//gs_draw_sprite(NULL, 0, 0,1000,1000); // Dibuja el sprite sin textura
//	}
//}
//
//
//// Función de inicialización del plugin
//bool obs_module_load(void)
//{
//	blog(LOG_INFO, "CARGANDO PLUGIN");
//	obs_register_source(&color_filter_info); // Registra la fuente en OBS
//	blog(LOG_INFO, "SE CARGO EL  PLUGIN [YULVEZ Y JOSE]");
//	return true;
//}
//
//// Función de limpieza del plugin
//void obs_module_unload(void)
//{
//	// Aquí puedes liberar recursos si es necesario
//}




////PARA C
#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("DSPStanky");
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("JOSESHEILAPLUGIN");
}

extern struct obs_source_info pixel_art_plugin;

bool obs_module_load(void)
{
	obs_register_source(&pixel_art_plugin);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}

//para c++
//#include <obs-module.h>
//#include "pixel_art_effect.hpp"
//
//OBS_DECLARE_MODULE()
//OBS_MODULE_USE_DEFAULT_LOCALE("pixel_art_plugin", "en-US")
//MODULE_EXPORT const char *obs_module_description(void)
//{
//	return "Plugin de Filtro Pixel Art";
//}
//
//bool obs_module_load(void)
//{
//	static struct obs_source_info pixel_art_filter = {
//		.id = "pixel_art_filter",
//		.type = OBS_SOURCE_TYPE_FILTER,
//		.output_flags = OBS_SOURCE_VIDEO,
//		.get_name = PixelArtPlugin::get_name,
//		.create = PixelArtPlugin::create,
//		.destroy = PixelArtPlugin::destroy,
//		.update = PixelArtPlugin::update,
//		.video_render=PixelArtPlugin::render,
//		
//	};
//
//	obs_register_source(&pixel_art_filter);
//	return true;
//}
