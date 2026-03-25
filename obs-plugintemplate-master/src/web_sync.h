
#ifndef WEB_SYNC_H
#define WEB_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Máximo de equipos que almacenamos del scoreboard */
#define MAX_SCOREBOARD_TEAMS 16 //valor arbitrario, se puede ajustar según necesidades

typedef struct scoreboard_team {
	char team_id[64];       /*ID del equipo (string en DOMjudge API) */
	char team_name[128];    /*Nombre del equipo (se rellena desde /teams si es necesario) */
	int  num_solved;        /** Problemas resueltos */
	int  total_time;        /**< Tiempo total de penalización */
	int  rank;              // Posición en el ranking 
} scoreboard_team_t;

/** Estructura opaca del sincronizador web */
typedef struct web_sync web_sync_t;

/**
 * Resultado de la última sincronización (para uso en el hilo principal).
 * Combina los datos de tiempo del concurso y del scoreboard.
 */
typedef struct web_sync_result {
	bool valid;              /**< true si hay datos nuevos y parseados correctamente */

	/* Datos del torneo */
	bool     contest_valid;      /**< true si se parseó contest correctamente */
	double   elapsed_seconds;    /**< Tiempo transcurrido desde start_time */
	double   remaining_seconds;  /**< Tiempo restante hasta end_time */
	double   total_duration;     /**< Duración total del torneo (end - start) */
	double   server_time;        /**< Hora del servidor (epoch) */

	/* Modo DOMjudge: datos del scoreboard */
	bool     scoreboard_valid;   /**< true si se parseó scoreboard */
	scoreboard_team_t teams[MAX_SCOREBOARD_TEAMS];
	int      team_count;         /**< Equipos válidos en el array */
} web_sync_result_t;


web_sync_t *web_sync_create_domjudge(const char *base_url,
				     const char *contest_id,
				     float interval_seconds);

/**
 * Destruye el sincronizador y detiene el hilo de peticiones.
 */
void web_sync_destroy(web_sync_t *sync);

/**
 * Actualiza base_url y contest_id para modo DOMjudge.
 */
void web_sync_set_contest(web_sync_t *sync, const char *base_url,
			  const char *contest_id);

/**
 * Actualiza el intervalo de peticiones (segundos).
 */
void web_sync_set_interval(web_sync_t *sync, float interval_seconds);

/**
 * Activa o desactiva las peticiones periódicas.
 */
void web_sync_set_enabled(web_sync_t *sync, bool enabled);

/**
 * Configura las credenciales para Autenticación Básica.
 * @param username Nombre de usuario (o NULL para desactivar)
 * @param password Contraseña
 */
void web_sync_set_auth(web_sync_t *sync, const char *username, const char *password);

/**
 * Consulta si hay un resultado nuevo (no bloquea).
 * Debe llamarse desde el hilo principal (p. ej. filter_tick).
 * Si hay resultado válido, lo copia a @a result y marca como consumido.
 * @return true si @a result contiene datos nuevos.
 */
bool web_sync_poll(web_sync_t *sync, web_sync_result_t *result);

/**
 * Hace una petición síncrona rápida para probar la conexión.
 * @param base_url URL base de la API
 * @param contest_id ID del torneo
 * @param username Usuario (opcional)
 * @param password Contraseña (opcional)
 * @return true si la conexión fue exitosa
 */
bool web_sync_test_connection(const char *base_url, const char *contest_id,
			      const char *username, const char *password);

/**
 * Copia la cache interna de equipos al array proporcionado.
 * @param sync Instancia del sincronizador
 * @param out_teams Array donde copiar los datos de equipos
 * @param max_teams Tamaño máximo del array
 * @return Número de equipos copiados
 */
int web_sync_get_teams(web_sync_t *sync, scoreboard_team_t *out_teams, int max_teams);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SYNC_H */
