/**
 * @file web_sync.c
 * @brief Implementación de la sincronización web con libcurl (hilo secundario) usando Windows threads.
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

#define WEB_SYNC_BUFFER_SIZE 4096
#define WEB_SYNC_MIN_INTERVAL 1.0f

//EJEMPLO CURL https://curl.se/libcurl/c/getinmemory.html PARA LA MEMORIA
struct web_sync {
	char *api_url;
	float interval_seconds; //cad cuanto se sincroniza, en segundos (mínimo 1s)
	volatile bool enabled; //volatile como en consolas
	volatile bool thread_stop;

	HANDLE thread;
	CRITICAL_SECTION mutex;

	bool has_new_result;
	uint32_t result_hours;
	uint32_t result_minutes;
	uint32_t result_seconds;
};


typedef struct {
	char *data;
	size_t size;
} memory_buffer_t;

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

		buf.size = 0;
		buf.data[0] = '\0';

		char url_copy[2048] = {0};
		EnterCriticalSection(&sync->mutex);
		if (sync->api_url)
			strncpy(url_copy, sync->api_url, sizeof(url_copy) - 1);
		LeaveCriticalSection(&sync->mutex);

		if (!url_copy[0]) {
			blog(LOG_WARNING,
			     "[WEB_SYNC] URL vacía, saltando petición");
			continue;
		}

		blog(LOG_INFO, "[WEB_SYNC] Realizando petición a URL: %s",
		     url_copy);

		curl_easy_setopt(curl, CURLOPT_URL, url_copy);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			blog(LOG_WARNING,
			     "[WEB_SYNC] curl_easy_perform falló: %s",
			     curl_easy_strerror(res));
			continue;
		}

		blog(LOG_INFO, "[WEB_SYNC] Respuesta recibida: %s", buf.data);

		uint32_t h = 0, m = 0, s = 0;
		
		bool ok = parse_json_int(buf.data, "hours", &h) &&
			  parse_json_int(buf.data, "minutes", &m) &&
			  parse_json_int(buf.data, "seconds", &s);

		if (ok) {
			blog(LOG_INFO,
			     "[WEB_SYNC] Valores parseados: %02u:%02u:%02u", h,
			     m, s);
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

void web_sync_destroy(web_sync_t *sync)
{
	if (!sync)
		return;
	sync->thread_stop = true;
	WaitForSingleObject(sync->thread, INFINITE);
	CloseHandle(sync->thread);
	DeleteCriticalSection(&sync->mutex);
	free(sync->api_url);
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
	result->valid = false;
	result->hours = 0;
	result->minutes = 0;
	result->seconds = 0;

	EnterCriticalSection(&sync->mutex);
	if (sync->has_new_result) {
		result->valid = true;
		result->hours = sync->result_hours;
		result->minutes = sync->result_minutes;
		result->seconds = sync->result_seconds;
		sync->has_new_result = false;
	}
	LeaveCriticalSection(&sync->mutex);

	return result->valid;
}
