
#ifndef WEB_SYNC_H
#define WEB_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Defines the maximum number of scoreboard teams to store.
// Define el numero maximo de equipos del scoreboard que se almacenan.
#define MAX_SCOREBOARD_TEAMS 16

typedef struct scoreboard_team {
	char team_id[64];
	char team_name[128];
	int num_solved;
	int total_time;
	int rank;
} scoreboard_team_t;

typedef struct web_sync web_sync_t;

typedef struct web_sync_result {
	bool valid;
	bool contest_valid;
	double elapsed_seconds;
	double remaining_seconds;
	double total_duration;
	double server_time;
	bool scoreboard_valid;
	scoreboard_team_t teams[MAX_SCOREBOARD_TEAMS];
	int team_count;
} web_sync_result_t;

// Creates the DOMjudge synchronization worker.
// Crea el sincronizador para DOMjudge.
web_sync_t *web_sync_create_domjudge(const char *base_url,
				     const char *contest_id,
				     float interval_seconds);

// Destroys the synchronizer and stops the worker thread.
// Destruye el sincronizador y detiene el hilo de trabajo.
void web_sync_destroy(web_sync_t *sync);

// Updates the contest endpoint configuration.
// Actualiza la configuracion del concurso.
void web_sync_set_contest(web_sync_t *sync, const char *base_url,
			  const char *contest_id);

// Updates the polling interval in seconds.
// Actualiza el intervalo de consulta en segundos.
void web_sync_set_interval(web_sync_t *sync, float interval_seconds);

// Enables or disables periodic requests.
// Activa o desactiva las peticiones periodicas.
void web_sync_set_enabled(web_sync_t *sync, bool enabled);

// Sets the basic authentication credentials.
// Configura las credenciales de autenticacion basica.
void web_sync_set_auth(web_sync_t *sync, const char *username, const char *password);

// Polls the latest synchronization result without blocking.
// Consulta el ultimo resultado de sincronizacion sin bloquear.
bool web_sync_poll(web_sync_t *sync, web_sync_result_t *result);

// Performs a synchronous connection test.
// Realiza una prueba sincrona de conexion.
bool web_sync_test_connection(const char *base_url, const char *contest_id,
			      const char *username, const char *password);

// Copies the cached teams into the provided array.
// Copia los equipos en cache al array proporcionado.
int web_sync_get_teams(web_sync_t *sync, scoreboard_team_t *out_teams, int max_teams);

/**
 * Copia el último resultado del scoreboard (con rank y num_solved) al array proporcionado.
 * A diferencia de web_sync_get_teams, incluye los datos de puntuación.
 * @param sync Instancia del sincronizador
 * @param out_teams Array donde copiar los datos
 * @param max_teams Tamaño máximo del array
 * @return Número de equipos copiados
 */
int web_sync_get_teams(web_sync_t *sync, scoreboard_team_t *out_teams, int max_teams);

/**
 * Copia el último resultado del scoreboard (con rank y num_solved) al array proporcionado.
 * A diferencia de web_sync_get_teams, incluye los datos de puntuación.
 * @param sync Instancia del sincronizador
 * @param out_teams Array donde copiar los datos
 * @param max_teams Tamaño máximo del array
 * @return Número de equipos copiados
 */
int web_sync_get_scoreboard(web_sync_t *sync, scoreboard_team_t *out_teams, int max_teams);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SYNC_H */
