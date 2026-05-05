#include "json_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Trims leading and trailing whitespace from a string in-place.
 *
 * Modifies the input string by removing any space, tab, or newline characters
 * from the beginning and end.
 *
 * @param str The null-terminated string to trim.
 * @return A pointer to the beginning of the trimmed string.
 */
char *trim_whitespace(char *str)
{
	if (!str) return NULL;
	char *end;
	while (isspace((unsigned char)*str)) str++;
	if (*str == 0) return str;
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;
	end[1] = '\0';
	return str;
}

/**
 * @brief Parses a non-negative integer value for a given key from a JSON string.
 *
 * Searches for a key in the format "key": value and extracts the integer.
 * This is a simple parser that does not handle nested structures or complex scenarios.
 *
 * @param json The JSON string to parse.
 * @param key The key to search for.
 * @param out_value Pointer to a uint32_t to store the parsed value.
 * @return `true` if the key and a valid integer value were found, `false` otherwise.
 */
bool parse_json_int(const char *json, const char *key,
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

/**
 * @brief Parses an integer from a nested JSON object.
 *
 * Finds a parent object and then searches for a child key within it to extract an integer value.
 * For example, it can parse `value` from `"parent_key": { ... "child_key": value ... }`.
 *
 * @param json The JSON string to parse.
 * @param parent_key The key of the parent object.
 * @param child_key The key of the integer value within the parent object.
 * @param out_value Pointer to a uint32_t to store the parsed value.
 * @return `true` if the nested key and value were found, `false` otherwise.
 */
bool parse_json_int_nested(const char *json, const char *parent_key,
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
 * @brief Parses a string value for a given key from a JSON string.
 *
 * Searches for a key in the format "key": "value" and copies the value into the
 * output buffer. It handles escaped quotes within the string.
 *
 * @param json The JSON string to parse.
 * @param key The key to search for.
 * @param out_str Buffer to store the parsed string.
 * @param out_size The size of the output buffer.
 * @return `true` if the key was found and the string was extracted, `false` otherwise.
 */
bool parse_json_string(const char *json, const char *key,
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
 * @brief Parses an ISO 8601 formatted date-time string to a Unix epoch time.
 *
 * Supports formats like:
 * - "2025-03-06T10:00:00+01:00"
 * - "2025-03-06T10:00:00.000+01:00"
 * - "2025-03-06T10:00:00Z"
 *
 * @param str The ISO 8601 string to parse.
 * @param out_epoch Pointer to a double to store the resulting epoch time (UTC).
 * @return `true` if parsing is successful, `false` otherwise.
 */
bool parse_iso8601(const char *str, double *out_epoch)
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
 * @brief Parses an HTTP 'Date' header string to a Unix epoch time.
 *
 * Supports the standard GMT format, e.g., "Thu, 06 Mar 2025 16:30:00 GMT".
 *
 * @param date_str The HTTP date string.
 * @param out_epoch Pointer to a double to store the resulting epoch time (UTC).
 * @return `true` if parsing is successful, `false` otherwise.
 */
bool parse_http_date(const char *date_str, double *out_epoch)
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
 * @brief Finds the beginning of the next JSON object '{...}' in a string.
 *
 * Useful for iterating through a JSON array of objects, such as scoreboard rows.
 *
 * @param p A pointer to the current position in the JSON string.
 * @return A pointer to the opening brace '{' of the next object, or NULL if not found.
 */
const char *find_next_json_object(const char *p)
{
	while (*p && *p != '{')
		p++;
	return (*p == '{') ? p : NULL;
}

/**
 * @brief Finds the corresponding closing brace '}' for a JSON object.
 *
 * This function correctly handles nested objects by counting brace depth.
 *
 * @param p A pointer to the opening brace '{' of a JSON object.
 * @return A pointer to the matching closing brace '}', or NULL if not found.
 */
const char *find_closing_brace(const char *p)
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
