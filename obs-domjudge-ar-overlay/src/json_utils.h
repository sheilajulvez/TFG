#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Trims leading and trailing whitespace in place.
// Recorta espacios en blanco al inicio y al final in situ.
char *trim_whitespace(char *str);

// Parses an unsigned integer value from JSON.
// Parsea un valor entero sin signo desde JSON.
bool parse_json_int(const char *json, const char *key, uint32_t *out_value);

// Parses an unsigned integer value from a nested JSON object.
// Parsea un valor entero sin signo desde un objeto JSON anidado.
bool parse_json_int_nested(const char *json, const char *parent_key,
			   const char *child_key, uint32_t *out_value);

// Parses a string value from JSON.
// Parsea un valor de cadena desde JSON.
bool parse_json_string(const char *json, const char *key, char *out_str,
		       size_t out_size);

// Parses an ISO 8601 date into UTC epoch seconds.
// Parsea una fecha ISO 8601 a segundos epoch UTC.
bool parse_iso8601(const char *str, double *out_epoch);

// Parses an HTTP Date header into UTC epoch seconds.
// Parsea una cabecera HTTP Date a segundos epoch UTC.
bool parse_http_date(const char *date_str, double *out_epoch);

// Finds the next JSON object in a string.
// Busca el siguiente objeto JSON en una cadena.
const char *find_next_json_object(const char *p);

// Finds the matching closing brace of a JSON object.
// Busca la llave de cierre correspondiente de un objeto JSON.
const char *find_closing_brace(const char *p);

#ifdef __cplusplus
}
#endif

#endif
