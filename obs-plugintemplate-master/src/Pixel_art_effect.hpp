#ifndef PLUGIN_EFFECT_H
#define PLUGIN_EFFECT_H

#include <obs-module.h>
#include <graphics/graphics.h> // Para usar los graficos de OBS

// Define la clase principal para nuestro efecto de plugin.
class PluginEffect {
public:
	// Constructor
	PluginEffect();
	// Destructor
	~PluginEffect();

	// Inicializa el efecto con la fuente de video.
	bool Initialize(obs_source_t *source);

	// Aplica el efecto de color en el frame actual.
	void ApplyEffect();

	// Actualiza la configuracion del efecto.
	void UpdateSettings(obs_data_t *settings);

	// Obtiene la configuracion por defecto para el efecto.
	static void GetDefaults(obs_data_t *settings);

	// Define las propiedades de UI para configurar el efecto.
	static obs_properties_t *GetProperties();

private:
	obs_source_t *
		m_source; // Puntero a la fuente de video a la que se aplica el filtro.
	uint32_t m_color;      // Color que se aplicara como overlay (ARGB).
	gs_effect_t *m_effect; // Puntero al objeto de efecto (shader) de OBS.
};

#endif // PLUGIN_EFFECT_H
