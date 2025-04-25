
//EFECTO BASICO QUE COMPILA PERO NO HACE NI MIERDAS



#include <obs-module.h>
#include <graphics/graphics.h>
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("simple_color_overlay", "en-US")

extern "C" {
static const char *plugin_effect_get_name(void *unused)
{
	return "Simple Color Overlay";
}

static void *plugin_effect_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);
	return source;
}

static void plugin_effect_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static void plugin_effect_update(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

static struct obs_source_frame * plugin_effect_filter_video(void *data, struct obs_source_frame *frame)
{
	if (!frame || frame->format != VIDEO_FORMAT_RGBA) {
		// sólo manejamos RGBA en este ejemplo
		return frame;
	}

	uint8_t *pixels = frame->data[0];
	uint32_t ls = frame->linesize[0];
	uint32_t w = frame->width;
	uint32_t h = frame->height;

	// Recorremos cada fila
	for (uint32_t y = 0; y < h; y++) {
		uint8_t *row = pixels + y * ls;
		// Cada píxel son 4 bytes: R, G, B, A (RGBA)
		for (uint32_t x = 0; x < w; x++) {
			uint8_t *px = row + x * 4;
			// Tintamos el canal R al 100%, y mezclamos 30%:
			px[0] = (uint8_t)(px[0] * 0.7f + 255 * 0.3f);
			// dejamos G,B,A igual
		}
	}

	return frame;
}

//static void ApplyEffect(void *data, gs_effect_t *effect)
//{
//	obs_source_t *source = static_cast<obs_source_t *>(data);
//
//	// Obtener la textura de la fuente (esto se realiza mediante la API correcta)
//	gs_texture_t *texture = obs_source_get_texture(source);
//	if (!texture)
//		return;
//
//	// Crear un efecto base para dibujar la textura
//	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
//	if (!solid)
//		return;
//
//	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
//
//	// Inicializa vec4 para definir el color
//	vec4 redColor = {1.0f, 0.0f, 0.0f, 0.3f}; // Rojo con transparencia
//
//	// Establecer el color en el shader
//	gs_effect_set_vec4(color, &redColor);
//
//	// Aplica el filtro (renderiza sobre la fuente original)
//	while (gs_effect_loop(solid, "Solid")) {
//		// Dibuja la textura de la fuente
//		gs_draw_sprite(texture, 0, 0, 0); // Dibuja la textura completa
//	}
//}

static struct obs_source_info plugin_effect_info = {
	.id = "simple_color_overlay",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,

	.get_name = plugin_effect_get_name,
	.create = plugin_effect_create,
	.destroy = plugin_effect_destroy,
	.video_tick = nullptr,
	.filter_video = plugin_effect_filter_video,

	.audio_render = nullptr};

bool obs_module_load(void)
{
	obs_register_source(&plugin_effect_info);
	return true;
}
}