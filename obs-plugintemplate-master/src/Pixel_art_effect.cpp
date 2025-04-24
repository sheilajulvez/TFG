#include "pixel_art_effect.hpp"
#include <obs-module.h>
PluginEffect::PluginEffect() : m_source(nullptr) {}

PluginEffect::~PluginEffect()
{
	// Limpiar recursos si es necesario
}

bool PluginEffect::Initialize(obs_source_t *source)
{
	m_source = source;
	return true;
}

void PluginEffect::ApplyEffect()
{
	if (!m_source) {
		return;
	}

	// Aquí puedes aplicar un filtro o efecto en el video.
	// Por ejemplo, ajustando el brillo o el contraste (esto es solo un ejemplo de efecto simple).
	gs_effect_t *effect =
		nullptr; // Aquí iría la lógica para crear el efecto.

	// Aplicar un efecto de ejemplo: por ejemplo, usando un shader simple.
	// Reemplaza este código con el efecto que desees aplicar.
}

// Función para crear el filtro
void *plugin_effect_create(obs_data_t *settings, obs_source_t *source)
{
	PluginEffect *effect = new PluginEffect();
	if (!effect->Initialize(source)) {
		delete effect;
		return nullptr;
	}
	return effect;
}

// Función para destruir el filtro
void plugin_effect_destroy(void *data)
{
	delete static_cast<PluginEffect *>(data);
}

// Función para obtener el nombre del filtro
const char *plugin_effect_get_name(void *data)
{
	return "Simple Color Effect";
}

// Función para renderizar el filtro (esto se ejecuta en cada frame)
void plugin_effect_video_render(void *data, gs_effect_t *effect)
{
	static_cast<PluginEffect *>(data)->ApplyEffect();
}

static struct obs_source_info plugin_effect_info = {
	.id = "plugin_effect",              // ID del filtro
	.type = OBS_SOURCE_TYPE_FILTER,     // Tipo de fuente
	.create = plugin_effect_create,     // Función para crear el filtro
	.destroy = plugin_effect_destroy,   // Función para destruir el filtro

};


// Registrar el filtro
bool obs_module_load()
{
	blog(LOG_INFO, "Plugin de efecto simple cargado.");
	plugin_effect_info.get_name =plugin_effect_get_name; // Función para obtener el nombre
	plugin_effect_info.video_render =plugin_effect_video_render; // Función para renderizar el video
	obs_register_source(&plugin_effect_info);
	return true;
}
