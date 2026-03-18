/**
 * @file json_utils.h
 * @brief Utilidades de parseo JSON y fechas (ISO 8601, HTTP Date).
 *
 * Funciones auxiliares extraídas de web_sync.c para reutilización
 * en otros módulos del plugin.
 */

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Recorta espacios en blanco al inicio y final de un string (in-place).
 * @return Puntero al inicio del string recortado.
 */
char *trim_whitespace(char *str);

/**
 * Parsea un entero no negativo después de "key": en el JSON.
 * Ejemplo: { "rank": 3 } → parse_json_int(json, "rank", &val) → val=3
 */
bool parse_json_int(const char *json, const char *key, uint32_t *out_value);

/**
 * Parsea un entero dentro de un objeto hijo en el JSON.
 * Ejemplo: { "score": { "num_solved": 5 } }
 *   → parse_json_int_nested(json, "score", "num_solved", &val)
 */
bool parse_json_int_nested(const char *json, const char *parent_key,
			   const char *child_key, uint32_t *out_value);

/**
 * Parsea un string JSON: "key": "value" y lo copia a out_str.
 * Maneja valores null y escapes con backslash.
 */
bool parse_json_string(const char *json, const char *key,
		       char *out_str, size_t out_size);

/**
 * Parsea una fecha ISO 8601 a epoch UTC (double).
 * Formatos soportados:
 *   "2025-03-06T10:00:00+01:00"
 *   "2025-03-06T10:00:00.000+01:00"
 *   "2025-03-06T10:00:00Z"
 */
bool parse_iso8601(const char *str, double *out_epoch);

/**
 * Parsea la cabecera HTTP Date: a epoch UTC.
 * Formato: "Thu, 06 Mar 2025 16:30:00 GMT"
 */
bool parse_http_date(const char *date_str, double *out_epoch);

/**
 * Busca el siguiente objeto '{...}' en un string JSON.
 * @return Puntero al '{' o NULL si no hay más.
 */
const char *find_next_json_object(const char *p);

/**
 * Busca el cierre '}' correspondiente contando niveles de anidación.
 * @return Puntero al '}' o NULL si no se encuentra.
 */
const char *find_closing_brace(const char *p);

#ifdef __cplusplus
}
#endif

#endif /* JSON_UTILS_H */
