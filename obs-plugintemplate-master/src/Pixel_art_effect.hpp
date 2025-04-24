#ifndef PLUGIN_EFFECT_H
#define PLUGIN_EFFECT_H

#include <obs-module.h>
#include <graphics/graphics.h>  // Para usar los gráficos de OBS

class PluginEffect {
public:
	PluginEffect();
	~PluginEffect();

	bool Initialize(obs_source_t *source);
	void ApplyEffect();

private:
	obs_source_t *m_source;
	uint32_t m_color;
};

#endif // PLUGIN_EFFECT_H
