/**
 * @file web_sync.c
 * @brief Implementación de la sincronización web con libcurl (hilo secundario).
 *
 * Soporta dos modos:
 * - Legacy: consulta una URL que devuelve { "hours", "minutes", "seconds" }
 * - DOMjudge: consulta /contests/{cid} y /contests/{cid}/scoreboard
 *   según la documentación de https://www.domjudge.org/demoweb/api/doc
 */

#include "web_sync.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <obs-module.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <curl/curl.h>

#define WEB_SYNC_BUFFER_SIZE 16384
#define WEB_SYNC_MIN_INTERVAL 1.0f

//EJEMPLO CURL https://curl.se/libcurl/c/getinmemory.html PARA LA MEMORIA
struct web_sync {
	char *api_url;
	float interval_seconds; //cad cuanto se sincroniza, en segundos (mínimo 1s)
	volatile bool enabled; //volatile como en consolas
	volatile bool thread_stop;

	HANDLE thread;
	CRITICAL_SECTION mutex;

	/* Modo legacy */
	bool has_new_result;
	uint32_t result_hours;
	uint32_t result_minutes;
	uint32_t result_seconds;

	/* Modo DOMjudge */
	bool domjudge_mode;
	char *base_url;
	char *contest_id;

	/* Resultados DOMjudge */
	bool has_contest_result;
	double result_elapsed_seconds;
	double result_remaining_seconds;

	/* Caché de equipos para nombres reales */
	scoreboard_team_t cached_teams[100];
	int cached_team_count;

	bool has_scoreboard_result;
	scoreboard_team_t result_teams[MAX_SCOREBOARD_TEAMS];
	int result_team_count;
};


typedef struct {
	char *data;
	size_t size;
} memory_buffer_t;

/* Buffer para almacenar la cabecera Date: del servidor */
typedef struct {
	char date_str[128];
	bool found;
} header_date_t;

static size_t write_callback(char *ptr, size_t size, size_t nmemb,void *userdata)
{
	memory_buffer_t *buf = (memory_buffer_t *)userdata;
	size_t total = size * nmemb;
	if (buf->size + total + 1 > WEB_SYNC_BUFFER_SIZE)
		total = WEB_SYNC_BUFFER_SIZE - buf->size - 1;
	if (total > 0) {
		memcpy(buf->data + buf->size, ptr, total);
		buf->size += total;
		buf->data[buf->size] = '\0';
	}
	return size * nmemb;
}

/**
 * Callback para interceptar la cabecera HTTP Date: de la respuesta.
 * Formato esperado: "Date: Thu, 06 Mar 2025 16:30:00 GMT\r\n"
 */
static size_t header_callback(char *buffer, size_t size, size_t nitems,
			      void *userdata)
{
	header_date_t *hdr = (header_date_t *)userdata;
	size_t total = size * nitems;

	/* Buscar "Date:" al inicio de la línea (case-insensitive) */
	if (total > 6 && (_strnicmp(buffer, "Date:", 5) == 0)) {
		const char *p = buffer + 5;
		while (*p == ' ' || *p == '\t')
			p++;

		size_t len = total - (size_t)(p - buffer);
		/* Eliminar \r\n del final */
		while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n'))
			len--;
		if (len >= sizeof(hdr->date_str))
			len = sizeof(hdr->date_str) - 1;
		memcpy(hdr->date_str, p, len);
		hdr->date_str[len] = '\0';
		hdr->found = true;
	}
	return total;
}

// https://medium.com/@priyanshugrv/building-a-simple-json-parser-in-c-9ecd1c6b1b9e parseador de json en C
/** Parsea un entero no negativo después de "key": en el JSON. */
static bool parse_json_int(const char *json, const char *key,
			   uint32_t *out_value)
{
	char search[64];
	snprintf(search, sizeof(search), "\"%s\"", key);
	const char *p = strstr(json, search);
	if (!p)
		return false;

	p += strlen(search);

	// Saltar espacios, tabs y saltos de línea
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;

	if (*p != ':')
		return false;
	p++;

	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;

	if (*p < '0' || *p > '9')
		return false;

	*out_value = 0;
	while (*p >= '0' && *p <= '9') {
		*out_value = *out_value * 10 + (uint32_t)(*p - '0');
		p++;
	}

	return true;
}

//https://en.cppreference.com/w/c/string/byte/strstr 
// /** Parsea un entero dentro de un objeto hijo en el JSON: "parent_key": { ... "child_key": value ... } */

// 
static bool parse_json_int_nested(const char *json, const char *parent_key,
				  const char *child_key, uint32_t *out_value)
{
	char search[128];
	snprintf(search, sizeof(search), "\"%s\"", parent_key);
	const char *p = strstr(json, search);
	if (!p)
		return false;

	const char *start = strchr(p, '{'); 
	if (!start)
		return false;

	return parse_json_int(start, child_key, out_value);
}

/**
 * Parsea un string JSON después de "key": "value" y lo copia a out_str.
 * Devuelve true si encontró la clave y extrajo el valor.
 */
static bool parse_json_string(const char *json, const char *key,
			      char *out_str, size_t out_size)
{
	char search[128];
	snprintf(search, sizeof(search), "\"%s\"", key);
	const char *p = strstr(json, search);
	if (!p)
		return false;

	p += strlen(search);

	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	if (*p != ':')
		return false;
	p++;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;

	/* Puede ser null */
	if (strncmp(p, "null", 4) == 0) {
		out_str[0] = '\0';
		return true;
	}

	if (*p != '"')
		return false;
	p++; /* saltar comilla de apertura */

	size_t i = 0;
	while (*p && *p != '"' && i < out_size - 1) {
		if (*p == '\\' && *(p + 1)) {
			p++; /* saltar backslash, copiar el siguiente char */
		}
		out_str[i++] = *p++;
	}
	out_str[i] = '\0';
	return (*p == '"');
}

/**
 * Parsea un timestamp ISO 8601 (formato DOMjudge API) a time_t.
 * Formatos soportados:
 *   "2025-03-06T10:00:00+01:00"
 *   "2025-03-06T10:00:00.000+01:00"
 *   "2025-03-06T10:00:00Z"
 */
static bool parse_iso8601(const char *str, double *out_epoch)
{
	if (!str || !str[0] || !out_epoch)
		return false;

	int year, month, day, hour, min, sec;
	int frac_ms = 0;
	int tz_hour = 0, tz_min = 0;
	char tz_sign = '+';

	/* Intentar parsear con fracción de segundo */
	int n = sscanf(str, "%d-%d-%dT%d:%d:%d.%d",
		       &year, &month, &day, &hour, &min, &sec, &frac_ms);
	if (n < 6) {
		n = sscanf(str, "%d-%d-%dT%d:%d:%d",
			   &year, &month, &day, &hour, &min, &sec);
		if (n < 6)
			return false;
	}

	/* Buscar zona horaria después de la T y los dígitos de tiempo */
	const char *tz = str;
	/* Avanzar hasta encontrar +, - o Z después de los segundos */
	while (*tz && *tz != '+' && *tz != 'Z') {
		if (*tz == '-' && tz > str + 10) /* ignorar guiones de fecha */
			break;
		tz++;
	}

	if (*tz == 'Z') {
		tz_hour = 0;
		tz_min = 0;
	} else if (*tz == '+' || *tz == '-') {
		tz_sign = *tz;
		sscanf(tz + 1, "%d:%d", &tz_hour, &tz_min);
	}

	/* Convertir a epoch UTC usando fórmula simplificada */
	struct tm tm_val;
	memset(&tm_val, 0, sizeof(tm_val));
	tm_val.tm_year = year - 1900;
	tm_val.tm_mon = month - 1;
	tm_val.tm_mday = day;
	tm_val.tm_hour = hour;
	tm_val.tm_min = min;
	tm_val.tm_sec = sec;
	tm_val.tm_isdst = 0;

	/* _mkgmtime en Windows convierte tm UTC a time_t */
	time_t epoch = _mkgmtime(&tm_val);
	if (epoch == (time_t)-1)
		return false;

	/* Ajustar por zona horaria: restamos el offset para pasar a UTC */
	int offset_seconds = (tz_hour * 3600 + tz_min * 60);
	if (tz_sign == '+')
		epoch -= offset_seconds;
	else
		epoch += offset_seconds;

	*out_epoch = (double)epoch;
	return true;
}

/**
 * Parsea la cabecera Date: HTTP (formato RFC 2822) a epoch UTC.
 * Formato: "Thu, 06 Mar 2025 16:30:00 GMT"
 */
static bool parse_http_date(const char *date_str, double *out_epoch)
{
	if (!date_str || !date_str[0] || !out_epoch)
		return false;

	/* Meses abreviados en inglés */
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May",
				       "Jun", "Jul", "Aug", "Sep", "Oct",
				       "Nov", "Dec"};

	int day, year, hour, min, sec;
	char month_str[8] = {0};

	/* "Thu, 06 Mar 2025 16:30:00 GMT" */
	int n = sscanf(date_str, "%*[^,], %d %3s %d %d:%d:%d",
		       &day, month_str, &year, &hour, &min, &sec);
	if (n < 6)
		return false;

	int month = -1;
	for (int i = 0; i < 12; i++) {
		if (_strnicmp(month_str, months[i], 3) == 0) {
			month = i;
			break;
		}
	}
	if (month < 0)
		return false;

	struct tm tm_val;
	memset(&tm_val, 0, sizeof(tm_val));
	tm_val.tm_year = year - 1900;
	tm_val.tm_mon = month;
	tm_val.tm_mday = day;
	tm_val.tm_hour = hour;
	tm_val.tm_min = min;
	tm_val.tm_sec = sec;
	tm_val.tm_isdst = 0;

	time_t epoch = _mkgmtime(&tm_val);
	if (epoch == (time_t)-1)
		return false;

	*out_epoch = (double)epoch;
	return true;
}

/**
 * Busca el siguiente objeto '{...}' en el array JSON de rows del scoreboard.
 * Devuelve puntero al '{' o NULL si no hay más.
 */
static const char *find_next_json_object(const char *p)
{
	while (*p && *p != '{')
		p++;
	return (*p == '{') ? p : NULL;
}

/**
 * Busca el cierre '}' correspondiente contando niveles de anidación.
 */
static const char *find_closing_brace(const char *p)
{
	if (*p != '{')
		return NULL;
	int depth = 0;
	while (*p) {
		if (*p == '{')
			depth++;
		else if (*p == '}') {
			depth--;
			if (depth == 0)
				return p;
		}
		p++;
	}
	return NULL;
}

/**
 * Parsea el JSON del scoreboard de DOMjudge.
 * Según la API: GET /api/v4/contests/{cid}/scoreboard
 * Devuelve objeto Scoreboard con:
 *   "rows": [ { "rank": 1, "team_id": "1", "score": { "num_solved": 3, "total_time": 120 }, ... } ]
 */
static int parse_scoreboard_json(const char *json,
				 scoreboard_team_t *teams, int max_teams)
{
	/* Buscar el array "rows" */
	const char *rows_key = strstr(json, "\"rows\"");
	if (!rows_key)
		return 0;

	/* Buscar el inicio del array '[' */
	const char *arr_start = strchr(rows_key, '[');
	if (!arr_start)
		return 0;

	int count = 0;
	const char *p = arr_start + 1;

	while (count < max_teams) {
		const char *obj = find_next_json_object(p);
		if (!obj)
			break;

		const char *obj_end = find_closing_brace(obj);
		if (!obj_end)
			break;

		/* Copiar el objeto a un buffer temporal para parseo seguro */
		size_t obj_len = (size_t)(obj_end - obj + 1);
		if (obj_len > 2048)
			obj_len = 2048;
		char temp[2048];
		memcpy(temp, obj, obj_len);
		temp[obj_len] = '\0';

		/* Extraer campos según la API DOMjudge */
		uint32_t rank_val = 0;
		char team_id_str[64] = {0};
		uint32_t num_solved = 0;
		uint32_t total_time_val = 0;

		parse_json_int(temp, "rank", &rank_val);
		parse_json_string(temp, "team_id", team_id_str,
				  sizeof(team_id_str));

		/* score es un objeto anidado: "score": { "num_solved": X, "total_time": Y } */
		parse_json_int_nested(temp, "score", "num_solved",
				      &num_solved);
		parse_json_int_nested(temp, "score", "total_time",
				      &total_time_val);

		strncpy(teams[count].team_id, team_id_str,
			sizeof(teams[count].team_id) - 1);
		teams[count].team_id[sizeof(teams[count].team_id) - 1] = '\0';

		/* El nombre se pone como "Team {id}" por defecto;
		   se puede enriquecer con /teams/{id} si es necesario */
		snprintf(teams[count].team_name,
			 sizeof(teams[count].team_name), "Team %s",
			 team_id_str);

		teams[count].num_solved = (int)num_solved;
		teams[count].total_time = (int)total_time_val;
		teams[count].rank = (int)rank_val;

		count++;
		p = obj_end + 1;

		/* Si encontramos ']' estamos fuera del array */
		while (*p && (*p == ' ' || *p == '\t' || *p == '\n' ||
			      *p == '\r' || *p == ','))
			p++;
		if (*p == ']')
			break;
	}

	return count;
}

/**
 * Parsea el JSON de equipos de DOMjudge.
 */
static int parse_teams_json(const char *json, scoreboard_team_t *teams, int max_teams)
{
	const char *arr_start = strchr(json, '[');
	if (!arr_start) return 0;
	int count = 0;
	const char *p = arr_start + 1;
	while (count < max_teams) {
		const char *obj = find_next_json_object(p);
		if (!obj) break;
		const char *obj_end = find_closing_brace(obj);
		if (!obj_end) break;
		size_t obj_len = (size_t)(obj_end - obj + 1);
		if (obj_len > 2048) obj_len = 2048;
		char temp[2048];
		memcpy(temp, obj, obj_len);
		temp[obj_len] = '\0';
		char tid[64] = {0};
		char name[128] = {0};
		parse_json_string(temp, "id", tid, sizeof(tid));
		if (!parse_json_string(temp, "display_name", name, sizeof(name)) || !name[0]) {
			parse_json_string(temp, "name", name, sizeof(name));
		}
		if (tid[0] && name[0]) {
			strncpy(teams[count].team_id, tid, sizeof(teams[count].team_id) - 1);
			strncpy(teams[count].team_name, name, sizeof(teams[count].team_name) - 1);
			count++;
		}
		p = obj_end + 1;
		while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) p++;
		if (*p == ']') break;
	}
	return count;
}

/**
 * Hilo principal de sincronización web.
 * Soporta modo legacy y modo DOMjudge.
 */
static DWORD WINAPI sync_thread_func(LPVOID arg)
{
	web_sync_t *sync = (web_sync_t *)arg;
	memory_buffer_t buf;
	buf.data = (char *)malloc(WEB_SYNC_BUFFER_SIZE);
	if (!buf.data) {
		blog(LOG_WARNING,
		     "[WEB_SYNC] Error: no se pudo asignar memoria para el buffer");
		return 0;
	}
	buf.size = 0;
	buf.data[0] = '\0';

	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_WARNING, "[WEB_SYNC] Error: curl_easy_init falló");
		free(buf.data);
		return 0;
	}

	blog(LOG_INFO, "[WEB_SYNC] Thread iniciado");

	while (!sync->thread_stop) {
		float interval = sync->interval_seconds;
		if (interval < WEB_SYNC_MIN_INTERVAL)
			interval = WEB_SYNC_MIN_INTERVAL;

		blog(LOG_DEBUG,
		     "[WEB_SYNC] Esperando %.2f segundos antes de la siguiente petición",
		     interval);
		Sleep((DWORD)(interval * 1000));

		if (sync->thread_stop) {
			blog(LOG_INFO, "[WEB_SYNC] Thread detenido");
			break;
		}

		if (!sync->enabled) {
			blog(LOG_DEBUG,
			     "[WEB_SYNC] Sincronización deshabilitada, saltando ciclo");
			continue;
		}

		/* Determinar modo de operación */
		bool is_domjudge = false;
		char contest_url[2048] = {0};
		char scoreboard_url[2048] = {0};
		char url_copy[2048] = {0};

		EnterCriticalSection(&sync->mutex);
		is_domjudge = sync->domjudge_mode;
		if (is_domjudge && sync->base_url && sync->contest_id) {
			snprintf(contest_url, sizeof(contest_url),
				 "%s/contests/%s", sync->base_url,
				 sync->contest_id);
			snprintf(scoreboard_url, sizeof(scoreboard_url),
				 "%s/contests/%s/scoreboard", sync->base_url,
				 sync->contest_id);
			static int teams_throttle = 0;
			if (sync->cached_team_count == 0 || teams_throttle++ % 10 == 0) {
				char teams_url[2048];
				snprintf(teams_url, sizeof(teams_url), "%s/contests/%s/teams", sync->base_url, sync->contest_id);
				
				/* Petición de equipos (fuera del mutex principal para no bloquear) */
				/* Usamos curls manuales o reutilizamos el objeto curl después de las otras peticiones */
			}
		} else if (sync->api_url) {
			strncpy(url_copy, sync->api_url,
				sizeof(url_copy) - 1);
		}
		LeaveCriticalSection(&sync->mutex);

		if (is_domjudge) {
			/* ============================================
			 * MODO DOMJUDGE: Petición al contest endpoint
			 * GET /api/v4/contests/{cid}
			 * ============================================ */
			if (!contest_url[0]) {
				blog(LOG_WARNING,
				     "[WEB_SYNC] URL de contest vacía");
				continue;
			}

			header_date_t hdr;
			memset(&hdr, 0, sizeof(hdr));

			buf.size = 0;
			buf.data[0] = '\0';

			blog(LOG_INFO,
			     "[WEB_SYNC] DOMjudge: petición contest a %s",
			     contest_url);

			curl_easy_setopt(curl, CURLOPT_URL, contest_url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
					 write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
					 header_callback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdr);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			/* DOMjudge puede requerir SSL */
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				blog(LOG_WARNING,
				     "[WEB_SYNC] curl contest falló: %s",
				     curl_easy_strerror(res));
				continue;
			}

			blog(LOG_INFO,
			     "[WEB_SYNC] Contest response: %.200s",
			     buf.data);

			/* Parsear hora del servidor desde la cabecera Date: */
			double server_time = 0;
			if (hdr.found) {
				if (!parse_http_date(hdr.date_str,
						     &server_time)) {
					blog(LOG_WARNING,
					     "[WEB_SYNC] No se pudo parsear Date: '%s'",
					     hdr.date_str);
					/* Fallback: usar hora local */
					server_time = (double)time(NULL);
				}
			} else {
				server_time = (double)time(NULL);
				blog(LOG_WARNING,
				     "[WEB_SYNC] No se encontró cabecera Date:, usando hora local");
			}

			/* Parsear start_time y end_time del JSON del contest */
			char start_time_str[128] = {0};
			char end_time_str[128] = {0};

			bool got_start = parse_json_string(
				buf.data, "start_time", start_time_str,
				sizeof(start_time_str));
			bool got_end = parse_json_string(
				buf.data, "end_time", end_time_str,
				sizeof(end_time_str));

			if (got_start && start_time_str[0]) {
				double start_epoch = 0, end_epoch = 0;
				if (parse_iso8601(start_time_str,
						  &start_epoch)) {
					double elapsed =
						server_time - start_epoch;
					if (elapsed < 0)
						elapsed = 0;

					double remaining = 0;
					if (got_end && end_time_str[0] &&
					    parse_iso8601(end_time_str,
							  &end_epoch)) {
						remaining =
							end_epoch - server_time;
						if (remaining < 0)
							remaining = 0;
					}

					/* Convertir remaining a H:M:S para compatibilidad con countdown_clock */
					uint32_t rem_total =
						(uint32_t)(remaining + 0.5);
					uint32_t h = rem_total / 3600;
					uint32_t m = (rem_total % 3600) / 60;
					uint32_t s = rem_total % 60;

					blog(LOG_INFO,
					     "[WEB_SYNC] Contest: elapsed=%.0fs, remaining=%02u:%02u:%02u",
					     elapsed, h, m, s);

					EnterCriticalSection(&sync->mutex);
					sync->has_new_result = true;
					sync->has_contest_result = true;
					sync->result_elapsed_seconds = elapsed;
					sync->result_remaining_seconds =
						remaining;
					sync->result_hours = h;
					sync->result_minutes = m;
					sync->result_seconds = s;
					LeaveCriticalSection(&sync->mutex);
				} else {
					blog(LOG_WARNING,
					     "[WEB_SYNC] No se pudo parsear start_time: '%s'",
					     start_time_str);
				}
			} else {
				blog(LOG_WARNING,
				     "[WEB_SYNC] No se encontró start_time en JSON del contest");
			}

			/* ============================================
			 * Petición al scoreboard endpoint 
			 * GET /api/v4/contests/{cid}/scoreboard
			 * ============================================ */
			if (scoreboard_url[0]) {
				/* Reset cabecera para la nueva petición */
				memset(&hdr, 0, sizeof(hdr));
				buf.size = 0;
				buf.data[0] = '\0';

				blog(LOG_INFO,
				     "[WEB_SYNC] DOMjudge: petición scoreboard a %s",
				     scoreboard_url);

				curl_easy_setopt(curl, CURLOPT_URL,
						 scoreboard_url);

				res = curl_easy_perform(curl);
				if (res != CURLE_OK) {
					blog(LOG_WARNING,
					     "[WEB_SYNC] curl scoreboard falló: %s",
					     curl_easy_strerror(res));
				} else {
					blog(LOG_INFO,
					     "[WEB_SYNC] Scoreboard response: %.200s",
					     buf.data);

					scoreboard_team_t teams
						[MAX_SCOREBOARD_TEAMS];
					int team_count =
						parse_scoreboard_json(
							buf.data, teams,
							MAX_SCOREBOARD_TEAMS);

					if (team_count > 0) {
						blog(LOG_INFO,
						     "[WEB_SYNC] Scoreboard: %d equipos parseados",
						     team_count);

						EnterCriticalSection(
							&sync->mutex);
						/* Enriquecer nombres desde la caché */
						for (int i = 0; i < team_count; i++) {
							for (int j = 0; j < sync->cached_team_count; j++) {
								if (strcmp(teams[i].team_id, sync->cached_teams[j].team_id) == 0) {
									strncpy(teams[i].team_name, sync->cached_teams[j].team_name, sizeof(teams[i].team_name)-1);
									break;
								}
							}
						}
						sync->has_scoreboard_result =
							true;
						sync->result_team_count =
							team_count;
						memcpy(sync->result_teams,
						       teams,
						       sizeof(scoreboard_team_t) *
							       team_count);
						LeaveCriticalSection(
							&sync->mutex);
					} else {
						blog(LOG_WARNING,
						     "[WEB_SYNC] No se parsearon equipos del scoreboard");
					}
				}
			}

			/* Petición de nombres de equipos (cache) */
			static int sync_teams_cycle = 0;
			if (sync_teams_cycle++ % 10 == 0) {
				char teams_url[2048] = {0};
				EnterCriticalSection(&sync->mutex);
				if (sync->base_url && sync->contest_id) {
					snprintf(teams_url, sizeof(teams_url), "%s/contests/%s/teams", sync->base_url, sync->contest_id);
				}
				LeaveCriticalSection(&sync->mutex);

				if (teams_url[0]) {
					buf.size = 0; buf.data[0] = '\0';
					curl_easy_setopt(curl, CURLOPT_URL, teams_url);
					if (curl_easy_perform(curl) == CURLE_OK) {
						scoreboard_team_t t_cache[100];
						int tc = parse_teams_json(buf.data, t_cache, 100);
						if (tc > 0) {
							EnterCriticalSection(&sync->mutex);
							sync->cached_team_count = tc;
							memcpy(sync->cached_teams, t_cache, sizeof(scoreboard_team_t) * tc);
							LeaveCriticalSection(&sync->mutex);
						}
					}
				}
			}

		} else {
			/* ============================================
			 * MODO LEGACY: JSON con hours/minutes/seconds
			 * ============================================ */
			if (!url_copy[0]) {
				blog(LOG_WARNING,
				     "[WEB_SYNC] URL vacía, saltando petición");
				continue;
			}

			buf.size = 0;
			buf.data[0] = '\0';

			blog(LOG_INFO,
			     "[WEB_SYNC] Realizando petición a URL: %s",
			     url_copy);

			curl_easy_setopt(curl, CURLOPT_URL, url_copy);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
					 write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, NULL);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				blog(LOG_WARNING,
				     "[WEB_SYNC] curl_easy_perform falló: %s",
				     curl_easy_strerror(res));
				continue;
			}

			blog(LOG_INFO, "[WEB_SYNC] Respuesta recibida: %s",
			     buf.data);

			uint32_t h = 0, m = 0, s = 0;

			bool ok = parse_json_int(buf.data, "hours", &h) &&
				  parse_json_int(buf.data, "minutes", &m) &&
				  parse_json_int(buf.data, "seconds", &s);

			if (ok) {
				blog(LOG_INFO,
				     "[WEB_SYNC] Valores parseados: %02u:%02u:%02u",
				     h, m, s);
				EnterCriticalSection(&sync->mutex);
				sync->has_new_result = true;
				sync->result_hours = h;
				sync->result_minutes = m;
				sync->result_seconds = s;
				LeaveCriticalSection(&sync->mutex);
			} else {
				blog(LOG_WARNING,
				     "[WEB_SYNC] Error al parsear JSON recibido");
			}
		}
	}

	blog(LOG_INFO, "[WEB_SYNC] Limpiando recursos del thread");
	curl_easy_cleanup(curl);
	free(buf.data);
	return 0;
}


web_sync_t *web_sync_create(const char *api_url, float interval_seconds)
{
	if (!api_url || interval_seconds < WEB_SYNC_MIN_INTERVAL)
		return NULL;

	web_sync_t *sync = (web_sync_t *)malloc(sizeof(web_sync_t));
	if (!sync)
		return NULL;
	memset(sync, 0, sizeof(web_sync_t));

	sync->api_url = _strdup(api_url);
	if (!sync->api_url) {
		free(sync);
		return NULL;
	}
	sync->interval_seconds = interval_seconds;
	sync->enabled = true;
	sync->thread_stop = false;
	sync->domjudge_mode = false;

	InitializeCriticalSection(&sync->mutex);

	sync->thread = CreateThread(NULL, 0, sync_thread_func, sync, 0, NULL);
	if (!sync->thread) {
		DeleteCriticalSection(&sync->mutex);
		free(sync->api_url);
		free(sync);
		return NULL;
	}

	return sync;
}

web_sync_t *web_sync_create_domjudge(const char *base_url,
				     const char *contest_id,
				     float interval_seconds)
{
	if (!base_url || !contest_id ||
	    interval_seconds < WEB_SYNC_MIN_INTERVAL)
		return NULL;

	web_sync_t *sync = (web_sync_t *)malloc(sizeof(web_sync_t));
	if (!sync)
		return NULL;
	memset(sync, 0, sizeof(web_sync_t));

	sync->domjudge_mode = true;
	sync->base_url = _strdup(base_url);
	sync->contest_id = _strdup(contest_id);
	if (!sync->base_url || !sync->contest_id) {
		free(sync->base_url);
		free(sync->contest_id);
		free(sync);
		return NULL;
	}

	sync->interval_seconds = interval_seconds;
	sync->enabled = true;
	sync->thread_stop = false;

	InitializeCriticalSection(&sync->mutex);

	sync->thread = CreateThread(NULL, 0, sync_thread_func, sync, 0, NULL);
	if (!sync->thread) {
		DeleteCriticalSection(&sync->mutex);
		free(sync->base_url);
		free(sync->contest_id);
		free(sync);
		return NULL;
	}

	return sync;
}

void web_sync_destroy(web_sync_t *sync)
{
	if (!sync)
		return;
	sync->thread_stop = true;
	WaitForSingleObject(sync->thread, INFINITE);
	CloseHandle(sync->thread);
	DeleteCriticalSection(&sync->mutex);
	free(sync->api_url);
	free(sync->base_url);
	free(sync->contest_id);
	free(sync);
}

void web_sync_set_url(web_sync_t *sync, const char *api_url)
{
	if (!sync || !api_url)
		return;
	EnterCriticalSection(&sync->mutex);
	free(sync->api_url);
	sync->api_url = _strdup(api_url);
	LeaveCriticalSection(&sync->mutex);
}

void web_sync_set_contest(web_sync_t *sync, const char *base_url,
			  const char *contest_id)
{
	if (!sync || !base_url || !contest_id)
		return;
	EnterCriticalSection(&sync->mutex);
	free(sync->base_url);
	free(sync->contest_id);
	sync->base_url = _strdup(base_url);
	sync->contest_id = _strdup(contest_id);
	sync->domjudge_mode = true;
	LeaveCriticalSection(&sync->mutex);
}

void web_sync_set_interval(web_sync_t *sync, float interval_seconds)
{
	if (!sync)
		return;
	if (interval_seconds < WEB_SYNC_MIN_INTERVAL)
		interval_seconds = WEB_SYNC_MIN_INTERVAL;
	sync->interval_seconds = interval_seconds;
}

void web_sync_set_enabled(web_sync_t *sync, bool enabled)
{
	if (sync)
		sync->enabled = enabled;
}

bool web_sync_poll(web_sync_t *sync, web_sync_result_t *result)
{
	if (!sync || !result)
		return false;

	/* Inicializar todo a cero/false */
	memset(result, 0, sizeof(web_sync_result_t));

	EnterCriticalSection(&sync->mutex);
	if (sync->has_new_result) {
		result->valid = true;
		result->hours = sync->result_hours;
		result->minutes = sync->result_minutes;
		result->seconds = sync->result_seconds;

		if (sync->has_contest_result) {
			result->contest_valid = true;
			result->elapsed_seconds =
				sync->result_elapsed_seconds;
			result->remaining_seconds =
				sync->result_remaining_seconds;
			sync->has_contest_result = false;
		}

		if (sync->has_scoreboard_result) {
			result->scoreboard_valid = true;
			result->team_count = sync->result_team_count;
			memcpy(result->teams, sync->result_teams,
			       sizeof(scoreboard_team_t) *
				       sync->result_team_count);
			sync->has_scoreboard_result = false;
		}

		sync->has_new_result = false;
	}
	LeaveCriticalSection(&sync->mutex);

	return result->valid;
}

bool web_sync_test_connection(const char *base_url, const char *contest_id)
{
	if (!base_url || !contest_id || !base_url[0] || !contest_id[0])
		return false;

	char url[2048];
	snprintf(url, sizeof(url), "%s/contests/%s", base_url, contest_id);

	CURL *curl = curl_easy_init();
	if (!curl)
		return false;

	memory_buffer_t buf;
	buf.data = (char *)malloc(WEB_SYNC_BUFFER_SIZE);
	if (!buf.data) {
		curl_easy_cleanup(curl);
		return false;
	}
	buf.size = 0;
	buf.data[0] = '\0';

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	CURLcode res = curl_easy_perform(curl);
	bool ok = false;
	if (res == CURLE_OK) {
		/* Verificar que el JSON contiene el campo "id" del contest */
		char id_str[64] = {0};
		ok = parse_json_string(buf.data, "id", id_str, sizeof(id_str));
		if (ok) {
			blog(LOG_INFO,
			     "[WEB_SYNC] Conexión OK: contest id='%s'",
			     id_str);
		} else {
			blog(LOG_WARNING,
			     "[WEB_SYNC] Respuesta recibida pero no parece un contest válido");
		}
	} else {
		blog(LOG_WARNING,
		     "[WEB_SYNC] Test conexión falló: %s",
		     curl_easy_strerror(res));
	}

	curl_easy_cleanup(curl);
	free(buf.data);
	return ok;
}
