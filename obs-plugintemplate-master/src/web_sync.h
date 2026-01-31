/**
 * @file web_sync.h
 * @brief Sincronización opcional del countdown desde una API REST.
 *
 * Consulta periódicamente una URL que devuelve JSON con { "hours", "minutes", "seconds" }
 * (tiempo restante). Las peticiones HTTP se ejecutan en un hilo secundario;
 * nunca se bloquea el hilo de render. El resultado se aplica como corrección
 * del reloj local (countdown_clock_sync_remaining).
 */

#ifndef WEB_SYNC_H
#define WEB_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Estructura opaca del sincronizador web */
typedef struct web_sync web_sync_t;

/**
 * Resultado de la última sincronización (para uso en el hilo principal).
 */
typedef struct web_sync_result {
	bool valid;           /**< true si hay datos nuevos y parseados correctamente */
	uint32_t hours;
	uint32_t minutes;
	uint32_t seconds;
} web_sync_result_t;

/**
 * Crea un sincronizador web.
 * @param api_url URL de la API REST (GET) que devuelve JSON con hours, minutes, seconds.
 * @param interval_seconds Intervalo entre peticiones en segundos (mínimo 1).
 * @return Instancia nueva o NULL si falla.
 */
web_sync_t *web_sync_create(const char *api_url, float interval_seconds);

/**
 * Destruye el sincronizador y detiene el hilo de peticiones.
 */
void web_sync_destroy(web_sync_t *sync);

/**
 * Actualiza la URL de la API (toma efecto en la siguiente petición).
 */
void web_sync_set_url(web_sync_t *sync, const char *api_url);

/**
 * Actualiza el intervalo de peticiones (segundos).
 */
void web_sync_set_interval(web_sync_t *sync, float interval_seconds);

/**
 * Activa o desactiva las peticiones periódicas.
 */
void web_sync_set_enabled(web_sync_t *sync, bool enabled);

/**
 * Consulta si hay un resultado nuevo (no bloquea).
 * Debe llamarse desde el hilo principal (p. ej. filter_tick).
 * Si hay resultado válido, lo copia a @a result y marca como consumido.
 * @return true si @a result contiene datos nuevos.
 */
bool web_sync_poll(web_sync_t *sync, web_sync_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SYNC_H */
