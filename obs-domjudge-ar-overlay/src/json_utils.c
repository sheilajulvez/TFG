#include "json_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Trims leading and trailing whitespace in place.
// Recorta espacios en blanco al inicio y al final in situ.
char *trim_whitespace(char *str)
{
	if (!str)
		return NULL;

	char *end;
	while (isspace((unsigned char)*str))
		str++;

	if (*str == '\0')
		return str;

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	end[1] = '\0';
	return str;
}

// Parses an unsigned integer value from JSON.
// Parsea un valor entero sin signo desde JSON.
bool parse_json_int(const char *json, const char *key, uint32_t *out_value)
{
	char search[64];
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

	if (*p < '0' || *p > '9')
		return false;

	*out_value = 0;
	while (*p >= '0' && *p <= '9') {
		*out_value = *out_value * 10 + (uint32_t)(*p - '0');
		p++;
	}

	return true;
}

// Parses an unsigned integer value from a nested JSON object.
// Parsea un valor entero sin signo desde un objeto JSON anidado.
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

// Parses a string value from JSON.
// Parsea un valor de cadena desde JSON.
bool parse_json_string(const char *json, const char *key, char *out_str,
		       size_t out_size)
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

	if (strncmp(p, "null", 4) == 0) {
		out_str[0] = '\0';
		return true;
	}

	if (*p != '"')
		return false;
	p++;

	size_t i = 0;
	while (*p && *p != '"' && i < out_size - 1) {
		if (*p == '\\' && *(p + 1))
			p++;
		out_str[i++] = *p++;
	}
	out_str[i] = '\0';
	return (*p == '"');
}

// Parses an ISO 8601 date into UTC epoch seconds.
// Parsea una fecha ISO 8601 a segundos epoch UTC.
bool parse_iso8601(const char *str, double *out_epoch)
{
	if (!str || !str[0] || !out_epoch)
		return false;

	int year, month, day, hour, min, sec;
	int frac_ms = 0;
	int tz_hour = 0, tz_min = 0;
	char tz_sign = '+';

	int n = sscanf(str, "%d-%d-%dT%d:%d:%d.%d", &year, &month, &day, &hour,
		       &min, &sec, &frac_ms);
	if (n < 6) {
		n = sscanf(str, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour,
			   &min, &sec);
		if (n < 6)
			return false;
	}

	const char *tz = str;
	while (*tz && *tz != '+' && *tz != 'Z') {
		if (*tz == '-' && tz > str + 10)
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

	struct tm tm_val;
	memset(&tm_val, 0, sizeof(tm_val));
	tm_val.tm_year = year - 1900;
	tm_val.tm_mon = month - 1;
	tm_val.tm_mday = day;
	tm_val.tm_hour = hour;
	tm_val.tm_min = min;
	tm_val.tm_sec = sec;
	tm_val.tm_isdst = 0;

	time_t epoch = _mkgmtime(&tm_val);
	if (epoch == (time_t)-1)
		return false;

	int offset_seconds = tz_hour * 3600 + tz_min * 60;
	if (tz_sign == '+')
		epoch -= offset_seconds;
	else
		epoch += offset_seconds;

	*out_epoch = (double)epoch;
	return true;
}

// Parses an HTTP Date header into UTC epoch seconds.
// Parsea una cabecera HTTP Date a segundos epoch UTC.
bool parse_http_date(const char *date_str, double *out_epoch)
{
	if (!date_str || !date_str[0] || !out_epoch)
		return false;

	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	int day, year, hour, min, sec;
	char month_str[8] = {0};

	int n = sscanf(date_str, "%*[^,], %d %3s %d %d:%d:%d", &day, month_str,
		       &year, &hour, &min, &sec);
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

// Finds the next JSON object in a string.
// Busca el siguiente objeto JSON en una cadena.
const char *find_next_json_object(const char *p)
{
	while (*p && *p != '{')
		p++;
	return (*p == '{') ? p : NULL;
}

// Finds the matching closing brace of a JSON object.
// Busca la llave de cierre correspondiente de un objeto JSON.
const char *find_closing_brace(const char *p)
{
	if (*p != '{')
		return NULL;

	int depth = 0;
	while (*p) {
		if (*p == '{') {
			depth++;
		} else if (*p == '}') {
			depth--;
			if (depth == 0)
				return p;
		}
		p++;
	}
	return NULL;
}
