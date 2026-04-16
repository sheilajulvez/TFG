

#include "web_sync.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <obs-module.h>
#include <util/bmem.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <curl/curl.h>
#include "json_utils.h"

#define WEB_SYNC_BUFFER_SIZE 16384
#define WEB_SYNC_MIN_INTERVAL 1.0f

//EJEMPLO CURL https://curl.se/libcurl/c/getinmemory.html PARA LA MEMORIA
struct web_sync {
	char *base_url;
	char *contest_id;
	float interval_seconds;
	volatile bool enabled;
	volatile bool thread_stop;

	HANDLE thread;
	CRITICAL_SECTION mutex;

	bool has_new_result;

	/* Resultados DOMjudge */
	bool has_contest_result;
	double result_elapsed_seconds;
	double result_remaining_seconds;
	double result_total_duration;
	double result_server_time;

	scoreboard_team_t team_names[100];
	int cached_team_count;

	/* Autenticación */
	char *username;
	char *password;

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
 *  "Date: Thu, 06 Mar 2025 16:30:00 GMT\r\n"
 */
static size_t header_callback(char *buffer, size_t size, size_t nitems,
			      void *userdata)
{
	header_date_t *hdr = (header_date_t *)userdata;
	size_t total = size * nitems;

	/* Buscar "Date:" al inicio de la línea  */
	if (total > 6 && (_strnicmp(buffer, "Date:", 5) == 0)) {
		const char *p = buffer + 5;
		while (*p == ' ' || *p == '\t')
			p++;

		size_t len = total - (size_t)(p - buffer);

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

static bool fetch_team_name_by_url(CURL *curl, memory_buffer_t *buf,
				   const char *team_url, char *out_name,
				   size_t out_name_size)
{
	if (!curl || !buf || !buf->data || !team_url || !team_url[0] ||
	    !out_name || out_name_size == 0)
		return false;

	out_name[0] = '\0';
	buf->size = 0;
	buf->data[0] = '\0';

	curl_easy_setopt(curl, CURLOPT_URL, team_url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (res != CURLE_OK || http_code != 200)
		return false;

	if (parse_json_string(buf->data, "display_name", out_name,
			      out_name_size) &&
	    out_name[0]) {
		return true;
	}
	if (parse_json_string(buf->data, "name", out_name, out_name_size) &&
	    out_name[0]) {
		return true;
	}
	return false;
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
		uint32_t team_id_num = 0;
		uint32_t num_solved = 0;
		uint32_t total_time_val = 0;

		parse_json_int(temp, "rank", &rank_val);
		if (!parse_json_string(temp, "team_id", team_id_str,
				       sizeof(team_id_str)) ||
		    !team_id_str[0]) {
			if (parse_json_int(temp, "team_id", &team_id_num)) {
				snprintf(team_id_str, sizeof(team_id_str), "%u",
					 team_id_num);
			}
		}

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
		uint32_t tid_num = 0;
		char name[128] = {0};
		if (!parse_json_string(temp, "id", tid, sizeof(tid)) || !tid[0]) {
			if (parse_json_int(temp, "id", &tid_num)) {
				snprintf(tid, sizeof(tid), "%u", tid_num);
			}
		}
		if (!parse_json_string(temp, "display_name", name, sizeof(name)) || !name[0]) {
			parse_json_string(temp, "name", name, sizeof(name));
		}
		if (tid[0] && name[0]) {
			/* Copia segura: garantiza terminador NUL para evitar basura en pantalla/logs. */
			strncpy(teams[count].team_id, tid, sizeof(teams[count].team_id) - 1);
			teams[count].team_id[sizeof(teams[count].team_id) - 1] = '\0';

			strncpy(teams[count].team_name, name,
				sizeof(teams[count].team_name) - 1);
			teams[count].team_name[sizeof(teams[count].team_name) - 1] =
				'\0';
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
 */
static DWORD WINAPI sync_thread_func(LPVOID arg)
{
	web_sync_t *sync = (web_sync_t *)arg;
	memory_buffer_t buf;
	CURL *curl = NULL;
	struct curl_version_info_data *vinfo = NULL;
	bool https_supported = false;
	const char *const *proto = NULL;
	
	/* Variables de bucle y estado */
	float interval;
	char contest_url[2048];
	char scoreboard_url[2048];
	char t_url[2048];
	char userpwd[256];
	char *auth_user = NULL;
	char *auth_pass = NULL;
	CURLcode res;
	long http_code;
	header_date_t hdr;
	
	/* Variables para parseo de contest */
	double server_time, start_epoch, end_epoch, elapsed, remaining, total_dur;
	char start_time_str[128];
	char end_time_str[128];
	bool got_start, got_end;
	
	/* Variables para scoreboard */
	scoreboard_team_t teams[MAX_SCOREBOARD_TEAMS];
	int team_count;
	int tc;
	scoreboard_team_t t_cache[100];
	 int teams_ = 0;
	 int teams_cycle_ = 0;

	buf.data = (char *)malloc(WEB_SYNC_BUFFER_SIZE);
	if (!buf.data) {
		blog(LOG_WARNING, "[WEB_SYNC] Error: no se pudo asignar memoria");
		return 0;
	}
	buf.size = 0;
	buf.data[0] = '\0';

	curl = curl_easy_init();
	if (!curl) {
		blog(LOG_WARNING, "[WEB_SYNC] Error: curl_easy_init falló");
		free(buf.data);
		return 0;
	}

	vinfo = curl_version_info(CURLVERSION_NOW);
	blog(LOG_INFO, "[WEB_SYNC] Thread iniciado. libcurl version: %s", vinfo->version);

	
	for (proto = vinfo->protocols; *proto; proto++) {
		if (_stricmp(*proto, "https") == 0) {
			https_supported = true;
			break;
		}
	}


	while (!sync->thread_stop) {
		interval = sync->interval_seconds;
		if (interval < WEB_SYNC_MIN_INTERVAL)
			interval = WEB_SYNC_MIN_INTERVAL;

		Sleep((DWORD)(interval * 1000));

		if (sync->thread_stop) break;
		if (!sync->enabled) continue;

		contest_url[0] = '\0';
		scoreboard_url[0] = '\0';
		auth_user = NULL;
		auth_pass = NULL;

		EnterCriticalSection(&sync->mutex);
		if (sync->base_url && sync->contest_id) {
			snprintf(contest_url, sizeof(contest_url), "%s/contests/%s", sync->base_url, sync->contest_id);
			snprintf(scoreboard_url, sizeof(scoreboard_url), "%s/contests/%s/scoreboard", sync->base_url, sync->contest_id);
			
			if (sync->cached_team_count == 0 || teams_++ % 10 == 0) {
				snprintf(t_url, sizeof(t_url), "%s/contests/%s/teams", sync->base_url, sync->contest_id);
			} else {
				t_url[0] = '\0';
			}
		}

		if (sync->username && sync->username[0]) {
			auth_user = bstrdup(sync->username);
			if (sync->password) auth_pass = bstrdup(sync->password);
		}
		LeaveCriticalSection(&sync->mutex);

		if (auth_user) {
			snprintf(userpwd, sizeof(userpwd), "%s:%s", auth_user, auth_pass ? auth_pass : "");
			curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
			curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
		} else {
			curl_easy_setopt(curl, CURLOPT_USERPWD, NULL);
		}

		if (contest_url[0]) {
			memset(&hdr, 0, sizeof(hdr));
			buf.size = 0; buf.data[0] = '\0';
			
			curl_easy_setopt(curl, CURLOPT_URL, contest_url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdr);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

			res = curl_easy_perform(curl);
			http_code = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

			if (res == CURLE_OK && http_code == 200) {
				server_time = 0;
				if (hdr.found) {
					if (!parse_http_date(hdr.date_str, &server_time)) {
						server_time = (double)time(NULL);
					}
				} else {
					server_time = (double)time(NULL);
				}

				got_start = parse_json_string(buf.data, "start_time", start_time_str, sizeof(start_time_str));
				got_end = parse_json_string(buf.data, "end_time", end_time_str, sizeof(end_time_str));

				if (got_start && start_time_str[0]) {
					if (parse_iso8601(start_time_str, &start_epoch)) {
						elapsed = server_time - start_epoch;
						if (elapsed < 0) elapsed = 0;
						
						remaining = 0;
						total_dur = 0;
						if (got_end && end_time_str[0] && parse_iso8601(end_time_str, &end_epoch)) {
							remaining = end_epoch - server_time;
							if (remaining < 0) remaining = 0;
							total_dur = end_epoch - start_epoch;
						}

						EnterCriticalSection(&sync->mutex);
						sync->has_new_result = true;
						sync->has_contest_result = true;
						sync->result_elapsed_seconds = elapsed;
						sync->result_remaining_seconds = remaining;
						sync->result_total_duration = total_dur;
						sync->result_server_time = server_time;
						LeaveCriticalSection(&sync->mutex);
					}
				}
			}
		}

		if (scoreboard_url[0]) {
			buf.size = 0; buf.data[0] = '\0';
			curl_easy_setopt(curl, CURLOPT_URL, scoreboard_url);
			res = curl_easy_perform(curl);
			if (res == CURLE_OK) {
				team_count = parse_scoreboard_json(buf.data, teams, MAX_SCOREBOARD_TEAMS);
				if (team_count > 0) {
					/* Resolver nombres desde cache local primero */
					EnterCriticalSection(&sync->mutex);
					for (int i = 0; i < team_count; i++) {
						for (int j = 0; j < sync->cached_team_count; j++) {
							if (strcmp(teams[i].team_id, sync->team_names[j].team_id) == 0) {
								strncpy(teams[i].team_name, sync->team_names[j].team_name, sizeof(teams[i].team_name)-1);
								teams[i].team_name[sizeof(teams[i].team_name) - 1] = '\0';
								break;
							}
						}
					}
					LeaveCriticalSection(&sync->mutex);

					/* Resolver nombres faltantes mediante /teams/{id} (top N mostrado). */
					char team_url[2048];
					char fetched_name[128];
					for (int i = 0; i < team_count; i++) {
						if (strncmp(teams[i].team_name, "Team ", 5) != 0)
							continue;
						if (!contest_url[0] || !teams[i].team_id[0])
							continue;

						snprintf(team_url, sizeof(team_url), "%s/teams/%s",
							 contest_url, teams[i].team_id);
						if (fetch_team_name_by_url(curl, &buf, team_url,
									 fetched_name, sizeof(fetched_name))) {
							strncpy(teams[i].team_name, fetched_name,
								sizeof(teams[i].team_name) - 1);
							teams[i].team_name[sizeof(teams[i].team_name) - 1] = '\0';

							/* Guardar en cache para no volver a pedirlo todo el rato. */
							EnterCriticalSection(&sync->mutex);
							bool found = false;
							for (int j = 0; j < sync->cached_team_count; j++) {
								if (strcmp(sync->team_names[j].team_id,
									   teams[i].team_id) == 0) {
									strncpy(sync->team_names[j].team_name,
										teams[i].team_name,
										sizeof(sync->team_names[j].team_name) - 1);
									sync->team_names[j].team_name[sizeof(sync->team_names[j].team_name) - 1] = '\0';
									found = true;
									break;
								}
							}
							if (!found && sync->cached_team_count < 100) {
								strncpy(sync->team_names[sync->cached_team_count].team_id,
									teams[i].team_id,
									sizeof(sync->team_names[sync->cached_team_count].team_id) - 1);
								sync->team_names[sync->cached_team_count].team_id[sizeof(sync->team_names[sync->cached_team_count].team_id) - 1] = '\0';
								strncpy(sync->team_names[sync->cached_team_count].team_name,
									teams[i].team_name,
									sizeof(sync->team_names[sync->cached_team_count].team_name) - 1);
								sync->team_names[sync->cached_team_count].team_name[sizeof(sync->team_names[sync->cached_team_count].team_name) - 1] = '\0';
								sync->cached_team_count++;
							}
							LeaveCriticalSection(&sync->mutex);
						}
					}

					EnterCriticalSection(&sync->mutex);
					sync->has_scoreboard_result = true;
					sync->result_team_count = team_count;
					memcpy(sync->result_teams, teams, sizeof(scoreboard_team_t) * team_count);
					LeaveCriticalSection(&sync->mutex);
				}
			}
		}

		if (teams_cycle_++ % 10 == 0) {
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
					tc = parse_teams_json(buf.data, t_cache, 100);
					if (tc > 0) {
						EnterCriticalSection(&sync->mutex);
						sync->cached_team_count = tc;
						memcpy(sync->team_names, t_cache, sizeof(scoreboard_team_t) * tc);
						LeaveCriticalSection(&sync->mutex);
					}
				}
			}
		}
		
		if (auth_user) bfree(auth_user);
		if (auth_pass) bfree(auth_pass);
	}

	curl_easy_cleanup(curl);
	free(buf.data);
	return 0;
}

void web_sync_destroy(web_sync_t *sync)
{
	if (!sync) return;
	sync->thread_stop = true;
	if (sync->thread) {
		WaitForSingleObject(sync->thread, INFINITE);
		CloseHandle(sync->thread);
	}
	DeleteCriticalSection(&sync->mutex);
	free(sync->base_url);
	free(sync->contest_id);
	free(sync->username);
	free(sync->password);
	free(sync);
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

void web_sync_set_contest(web_sync_t *sync, const char *base_url,
			  const char *contest_id)
{
	if (!sync || !base_url || !contest_id)
		return;

	EnterCriticalSection(&sync->mutex);
	if (sync->base_url) free(sync->base_url);
	if (sync->contest_id) free(sync->contest_id);
	sync->base_url = _strdup(base_url);
	sync->contest_id = _strdup(contest_id);
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

	bool has_new = false;
	EnterCriticalSection(&sync->mutex);

	if (sync->has_new_result) {
		memset(result, 0, sizeof(web_sync_result_t));
		result->valid = true;

		if (sync->has_contest_result) {
			result->contest_valid = true;
			result->elapsed_seconds = sync->result_elapsed_seconds;
			result->remaining_seconds =
				sync->result_remaining_seconds;
			result->total_duration = sync->result_total_duration;
			result->server_time = sync->result_server_time;
		}

		if (sync->has_scoreboard_result) {
			result->scoreboard_valid = true;
			result->team_count = sync->result_team_count;
			memcpy(result->teams, sync->result_teams,
			       sizeof(scoreboard_team_t) *
				       sync->result_team_count);
		}

		sync->has_new_result = false;
		has_new = true;
	}

	LeaveCriticalSection(&sync->mutex);
	return has_new;
}

void web_sync_set_auth(web_sync_t *sync, const char *username, const char *password)
{
	if (!sync) return;
	EnterCriticalSection(&sync->mutex);
	if (sync->username) free(sync->username);
	if (sync->password) free(sync->password);
	sync->username = (username && username[0]) ? _strdup(username) : NULL;
	sync->password = (password && password[0]) ? _strdup(password) : NULL;
	LeaveCriticalSection(&sync->mutex);
}

bool web_sync_test_connection(const char *base_url, const char *contest_id,
			      const char *username, const char *password)
{
	long http_code = 0;
	CURLcode res;
	bool ok = false;
	char url[2048];
	CURL *curl;
	memory_buffer_t buf;

	if (!base_url || !contest_id || !base_url[0] || !contest_id[0])
		return false;

	snprintf(url, sizeof(url), "%s/contests/%s", base_url, contest_id);

	curl = curl_easy_init();
	if (!curl)
		return false;

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

	if (username && username[0]) {
		char auth[256];
		snprintf(auth, sizeof(auth), "%s:%s", username, password ? password : "");
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
	}

	res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	if (res == CURLE_OK) {
		/* Verificar que el JSON contiene el campo "id" del contest */
		char id_str[64] = {0};
		ok = parse_json_string(buf.data, "id", id_str, sizeof(id_str));
		if (ok) {
			blog(LOG_INFO,
			     "[WEB_SYNC] Conexión OK: contest id='%s' (HTTP %ld)",
			     id_str, http_code);
		} else {
			char preview[129] = {0};
			if (buf.data) {
				strncpy(preview, buf.data, 128);
				preview[128] = '\0';
			}
			blog(LOG_WARNING,
			     "[WEB_SYNC] Respuesta recibida (HTTP %ld) pero no parece un contest válido. "
			     "Inicio de respuesta: %s",
			     http_code, preview);
		}
	} else {
		blog(LOG_WARNING,
		     "[WEB_SYNC] Test conexión falló (CURL %d): %s",
		     (int)res, curl_easy_strerror(res));
	}

	curl_easy_cleanup(curl);
	free(buf.data);
	return ok;
}

int web_sync_get_teams(web_sync_t *sync, scoreboard_team_t *out_teams, int max_teams)
{
	if (!sync || !out_teams || max_teams <= 0)
		return 0;

	int count = 0;
	EnterCriticalSection(&sync->mutex);
	count = sync->cached_team_count;
	if (count > max_teams)
		count = max_teams;
	if (count > 0)
		memcpy(out_teams, sync->team_names, sizeof(scoreboard_team_t) * count);
	LeaveCriticalSection(&sync->mutex);
	return count;
}

