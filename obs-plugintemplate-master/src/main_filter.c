// Include the OBS Studio API headers.
// Inclusion de las cabeceras del API de OBS Studio.
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <graphics/vec4.h>

// Include standard C libraries.
// Inclusion de bibliotecas estandar de C.
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "SJ_3DModel.h"
#include "countdown_clock.h"
#include "web_sync.h"
#include "aruco_detector.h"

// Para M_PI en Windows.
// For M_PI on Windows
#define _USE_MATH_DEFINES
#define DEGREES_TO_RADIANS(angle) ((angle) * (float)M_PI / 180.0f)
#include <math.h>
#include <curl/curl.h>
#include "json_utils.h"


/* Maximum number of ArUco marker to team_id mappings for Team Info mode. */
/* Maximo de mappings ArUco marker â†’ team_id para modo Team Info */
#define MAX_TEAM_INF 16

struct team_info_mapping {
	int aruco_id;     /* ID del marker ArUco detectado */
	char team_id[64]; /* ID de equipo (DOMjudge) asociado */
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("cube", "en-US")

/* Returns the module description exposed to OBS. */
/* Devuelve la descripcion del modulo expuesta a OBS. */
MODULE_EXPORT const char *obs_module_description(void)
{
	return "SJ_3D";
}
/* Converts an angle from degrees to radians. */
/* Convierte un angulo de grados a radianes. */
static inline float degrees_to_radians(float degrees)
{
	return degrees * (float)M_PI / 180.0f;
}

/* Clamps a floating-point value to the specified range. */
/* Limita un valor de punto flotante al rango especificado. */
static inline float clampf(float v, float lo, float hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

/* Clamps an integer value to the specified range. */
/* Limita un valor entero al rango especificado. */
static inline int clampi(int v, int lo, int hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

/* Helpers UTF-8 (simples) para truncar sin cortar bytes a mitad de caracter.
 * NOTA: OBS usa UTF-8 en cadenas, pero aqui evitamos depender de librerias externas. */
/* Returns the byte length of the next UTF-8 code point. */
/* Devuelve la longitud en bytes del siguiente punto de codigo UTF-8. */
static size_t utf8_next_char_len(const char *s)
{
	if (!s || !s[0])
		return 0;

	const unsigned char c = (unsigned char)s[0];
	if (c < 0x80)
		return 1;
	if ((c & 0xE0) == 0xC0)
		return (s[1] ? 2 : 1);
	if ((c & 0xF0) == 0xE0)
		return (s[1] && s[2]) ? 3 : 1;
	if ((c & 0xF8) == 0xF0)
		return (s[1] && s[2] && s[3]) ? 4 : 1;
	return 1;
}

/* Counts UTF-8 code points up to the specified limit. */
/* Cuenta puntos de codigo UTF-8 hasta el limite especificado. */
static int utf8_count_codepoints_limit(const char *s, int max_codepoints)
{
	if (!s || max_codepoints <= 0)
		return 0;

	int count = 0;
	const char *p = s;
	while (*p && count < max_codepoints) {
		size_t n = utf8_next_char_len(p);
		if (n == 0)
			break;
		p += n;
		count++;
	}
	return count;
}

/* Copies a UTF-8 string and appends an ellipsis when truncation is required. */
/* Copia una cadena UTF-8 y agrega puntos suspensivos cuando es necesario truncar. */
static int utf8_copy_trunc_ellipsis(const char *in, char *out, size_t out_size,
				   int max_chars)
{
	if (!out || out_size == 0)
		return 0;
	out[0] = '\0';

	if (!in || !in[0] || max_chars <= 0)
		return 0;

	int copied = 0;
	size_t oi = 0;
	const char *p = in;
	while (*p && copied < max_chars) {
		size_t n = utf8_next_char_len(p);
		if (n == 0)
			break;
		if (oi + n + 1 >= out_size)
			break;
		memcpy(out + oi, p, n);
		oi += n;
		p += n;
		copied++;
	}
	out[oi] = '\0';

	if (*p) {
		const char *dots = "...";
		size_t dots_len = 3;
		if (out_size >= dots_len + 1) {
			/* Si no hay espacio, recortar para meter puntos. */
/* If there is not enough space, trim to fit the ellipsis. */
			while (oi + dots_len + 1 >= out_size && oi > 0) {
				/* Retroceder un caracter UTF-8 completo. */
/* Move back one complete UTF-8 character. */
				oi--;
				while (oi > 0 && ((unsigned char)out[oi] & 0xC0) == 0x80)
					oi--;
			}
			if (oi + dots_len + 1 < out_size) {
				memcpy(out + oi, dots, dots_len);
				oi += dots_len;
				out[oi] = '\0';
				return 1;
			}
		}
	}

	return 0;
}

/* Replaces control characters with spaces for safe scoreboard rendering. */
/* Reemplaza caracteres de control por espacios para un renderizado seguro del scoreboard. */
static void scoreboard_sanitize_name(const char *in, char *out, size_t out_size)
{
	if (!out || out_size == 0) {
		return;
	}
	out[0] = '\0';
	if (!in || !in[0]) {
		return;
	}

	size_t oi = 0;
	for (size_t i = 0; in[i] && oi + 1 < out_size; i++) {
		unsigned char c = (unsigned char)in[i];
		if (c == '\r' || c == '\n' || c == '\t') {
			out[oi++] = ' ';
			continue;
		}
		if (c < 32) {
			out[oi++] = ' ';
			continue;
		}
		out[oi++] = (char)c;
	}
	out[oi] = '\0';
}

/* Selects a high-contrast foreground color for the given background color. */
/* Selecciona un color de primer plano con alto contraste para el color de fondo dado. */
static void overlay_pick_contrast_rgb(float r, float g, float b, float *out_r,
				      float *out_g, float *out_b)
{
	const float l = 0.2126f * r + 0.7152f * g + 0.0722f * b;
	if (l < 0.5f) {
		*out_r = 1.0f;
		*out_g = 1.0f;
		*out_b = 1.0f;
	} else {
		*out_r = 0.0f;
		*out_g = 0.0f;
		*out_b = 0.0f;
	}
}

/* Copies up to the requested number of UTF-8 code points safely. */
/* Copia de forma segura hasta la cantidad solicitada de puntos de codigo UTF-8. */
static size_t utf8_copy_n_codepoints(const char *in, char *out, size_t out_size,
				     size_t max_codepoints, bool *out_truncated)
{
	/* Copia hasta max_codepoints respetando limites de UTF-8 (no corta multibyte).
	 * Devuelve bytes escritos (sin incluir '\0').
	 */
	if (!out || out_size == 0) {
		if (out_truncated)
			*out_truncated = false;
		return 0;
	}
	out[0] = '\0';

	if (!in || !in[0] || max_codepoints == 0) {
		if (out_truncated)
			*out_truncated = false;
		return 0;
	}

	size_t written = 0;
	size_t cp = 0;
	const unsigned char *p = (const unsigned char *)in;

	while (*p && cp < max_codepoints) {
		size_t seq = 1;
		if ((*p & 0x80) == 0x00) {
			seq = 1;
		} else if ((*p & 0xE0) == 0xC0) {
			seq = 2;
		} else if ((*p & 0xF0) == 0xE0) {
			seq = 3;
		} else if ((*p & 0xF8) == 0xF0) {
			seq = 4;
		} else {
			seq = 1;
		}

		if (written + seq >= out_size)
			break;

		for (size_t i = 0; i < seq; i++) {
			if (!p[i])
				break;
			out[written++] = (char)p[i];
		}
		out[written] = '\0';

		p += seq;
		cp++;
	}

	const bool truncated = (*p != '\0');
	if (out_truncated)
		*out_truncated = truncated;
	return written;
}

/* Truncates a UTF-8 string and appends an ellipsis when needed. */
/* Trunca una cadena UTF-8 y agrega puntos suspensivos cuando es necesario. */
static void utf8_truncate_with_ellipsis(const char *in, char *out, size_t out_size,
				       size_t max_codepoints)
{
	if (max_codepoints <= 3) {
		bool dummy = false;
		utf8_copy_n_codepoints(in, out, out_size, max_codepoints, &dummy);
		return;
	}

	size_t in_codepoints = 0;
	const unsigned char *pi = (const unsigned char *)in;
	while (*pi) {
		if ((*pi & 0xC0) != 0x80) in_codepoints++;
		pi++;
	}

	if (in_codepoints <= max_codepoints) {
		bool dummy = false;
		utf8_copy_n_codepoints(in, out, out_size, max_codepoints, &dummy);
	} else {
		bool dummy = false;
		size_t keep = max_codepoints - 3;
		utf8_copy_n_codepoints(in, out, out_size, keep, &dummy);
		
		const char *ellipsis = "...";
		const size_t el_len = 3;
		const size_t cur = strlen(out);
		if (cur + el_len < out_size) {
			memcpy(out + cur, ellipsis, el_len);
			out[cur + el_len] = '\0';
		}
	}
}

/* Counts lines using LF as the line separator. */
/* Cuenta las lineas usando LF como separador de linea. */
static int count_lines_lf(const char *text)
{
	if (!text || !text[0])
		return 0;
	int lines = 1;
	for (const char *p = text; *p; p++) {
		if (*p == '\n')
			lines++;
	}
	return lines;
}

/* Converts an ArUco rotation vector into a 3x3 rotation matrix. */
/* Convierte un vector de rotacion de ArUco en una matriz de rotacion 3x3. */
static void aruco_rvec_to_rotmat3x3(const float rvec[3], float R[3][3])
{
	const float rx = rvec[0];
	const float ry = rvec[1];
	const float rz = rvec[2];
	const float theta = sqrtf(rx * rx + ry * ry + rz * rz);

	if (theta < 1e-6f) {
// Identity matrix...
		// Matriz identidad...
		R[0][0] = 1.0f;
		R[0][1] = 0.0f;
		R[0][2] = 0.0f;
		R[1][0] = 0.0f;
		R[1][1] = 1.0f;
		R[1][2] = 0.0f;
		R[2][0] = 0.0f;
		R[2][1] = 0.0f;
		R[2][2] = 1.0f;
		return;
	}
/* Axis inversion to match the negative X scaling. */
	/*  Inversión de ejes para coordinar con el escalado X negativo */
	const float kx = rx / theta;
	const float ky = -(ry /theta); // Invertimos Y para que el giro horizontal sea correcto
	const float kz = -(rz /	theta);// Invertimos Z para que el giro de volante sea correcto

	const float c = cosf(theta);
	const float s = sinf(theta);
	const float v = 1.0f - c;

	R[0][0] = kx * kx * v + c;
	R[0][1] = kx * ky * v - kz * s;
	R[0][2] = kx * kz * v + ky * s;

	R[1][0] = ky * kx * v + kz * s;
	R[1][1] = ky * ky * v + c;
	R[1][2] = ky * kz * v - kx * s;

	R[2][0] = kz * kx * v - ky * s;
	R[2][1] = kz * ky * v + kx * s;
	R[2][2] = kz * kz * v + c;
}

/* Builds a 4x4 pose matrix from ArUco rotation and translation vectors. */
/* Construye una matriz de pose 4x4 a partir de los vectores de rotacion y traslacion de ArUco. */
static void aruco_pose_to_matrix4(const float rvec[3], const float tvec[3],
				  struct matrix4 *out_pose)
{
	if (!out_pose)
		return;

	float R3[3][3];
	aruco_rvec_to_rotmat3x3(rvec, R3);

	matrix4_identity(out_pose);
	out_pose->x.x = R3[0][0];
	out_pose->x.y = R3[1][0];
	out_pose->x.z = R3[2][0];
	out_pose->x.w = 0.0f;

	out_pose->y.x = R3[0][1];
	out_pose->y.y = R3[1][1];
	out_pose->y.z = R3[2][1];
	out_pose->y.w = 0.0f;

	out_pose->z.x = R3[0][2];
	out_pose->z.y = R3[1][2];
	out_pose->z.z = R3[2][2];
	out_pose->z.w = 0.0f;

	out_pose->t.x = tvec[0];
	out_pose->t.y = tvec[1];
	out_pose->t.z = tvec[2];
	out_pose->t.w = 1.0f;
}

/* Computes stable 2D marker metrics for screen-space alignment. */
/* Calcula metricas 2D estables del marcador para alineacion en espacio de pantalla. */
static bool aruco_marker_metrics_2d(const ArucoResult *res,
				    float screen_h,
				    float *out_edge_px,
				    float *out_width_px,
				    float *out_height_px,
				    float *out_angle_rad,
				    float *out_center_x,
				    float *out_center_y)
{
	if (!res || !res->detected || !(screen_h > 1.0f) || !out_edge_px || !out_angle_rad ||
	    !out_width_px || !out_height_px || !out_center_x || !out_center_y)
		return false;

/* Raw marker coordinates (Y=0 is the top in cameras and OBS). */
	/* Coordenadas RAW del marcador (Y=0 es la parte superior en cámaras y OBS) */
	const float x0 = res->corners[0][0], y0 = res->corners[0][1];
	const float x1 = res->corners[1][0], y1 = res->corners[1][1];
	const float x2 = res->corners[2][0], y2 = res->corners[2][1];
	const float x3 = res->corners[3][0], y3 = res->corners[3][1];

	const float e01 = hypotf(x1 - x0, y1 - y0);
	const float e12 = hypotf(x2 - x1, y2 - y1);
	const float e23 = hypotf(x3 - x2, y3 - y2);
	const float e30 = hypotf(x0 - x3, y0 - y3);
	const float marker_w = (e01 + e23) * 0.5f;
	const float marker_h = (e12 + e30) * 0.5f;

	const float edge_avg = (e01 + e12 + e23 + e30) * 0.25f;
	if (!(edge_avg > 1.0f) || !(marker_w > 1.0f) || !(marker_h > 1.0f))
		return false;

/* Marker center (average of corners for maximum horizontal stability). */
	/* Centro del marcador (promedio de esquinas para máxima estabilidad horizontal) */
	*out_center_x = (x0 + x1 + x2 + x3) * 0.25f;
	*out_center_y = (y0 + y1 + y2 + y3) * 0.25f;

	/* Angulo robusto en pantalla: promedio de borde superior e inferior. */
/* Robust on-screen angle: average of the top and bottom edges. */
	const float vx = ((x1 - x0) + (x2 - x3)) * 0.5f;
	const float vy = ((y1 - y0) + (y2 - y3)) * 0.5f;
	*out_angle_rad = atan2f(vy, vx);
	*out_edge_px = edge_avg;
	*out_width_px = marker_w;
	*out_height_px = marker_h;
	return true;
}
struct cube_filter_data {
	gs_zstencil_t *zstencil;
	obs_source_t *source;
	gs_texture_t *texture;
	float width_screen;
	float height_screen;

	/* Vertex buffer de un quad unidad (0..1) para dibujar fondos 2D sin recrear geometria por frame */
	gs_vertbuffer_t *overlay_bg_vb;
	/* Vertex buffer del fondo redondeado ya triangulado en pixeles (se regenera si cambia w/h/radio) */
	gs_vertbuffer_t *overlay_bg_round_vb;
	float overlay_bg_round_last_w;
	float overlay_bg_round_last_h;
	int overlay_bg_round_last_radius;

	float *model_width;
	float *model_height;

	struct Mesh *g_meshes;
	size_t g_mesh_count;
	char *model_path_str;
	char *texture_path_str;
	// Parametros de posicion / escala / rotacion manual
// Manual position / scale / rotation parameters.
	float pos_x;
	float pos_y;
	float pos_z;
	float scale;
	float current_scale;
	float rotation_x;
	float rotation_y;
	float rotation_z;
	gs_texture_t *loaded_texture;


	float ar_offset_pos_x;
	float ar_offset_pos_y;
	float ar_offset_pos_z;
	float ar_offset_rot_x;
	float ar_offset_rot_y;
	float ar_offset_rot_z;
	

	ArucoDetector *detector; //
	ArucoResult last_result; //

	int mode;


	countdown_clock_t *countdown_clock;
	web_sync_t *web_sync;
	bool countdown_running;
	bool countdown_reset_requested;
	uint32_t countdown_duration_h;
	uint32_t countdown_duration_m;
	uint32_t countdown_duration_s;
	bool sync_enabled;
	float sync_interval_sec;
	char *api_username;
	char *api_password;

/* DOMjudge API. */
	/* DOMjudge API */
	char *api_base_url;          // URL base API DOMjudge (ej. https://servidor.com/api/v4)
	char *contest_id;            // ID del torneo (ej. "2" o "demo")

/* Scoreboard data (protected: written only in `filter_tick`). */
	/* Scoreboard data (protegido: solo se escribe en filter_tick) */
	scoreboard_team_t scoreboard_teams[MAX_SCOREBOARD_TEAMS];
	int scoreboard_team_count;
	obs_source_t *scoreboard_text_source;  
	/* Modo 3 (Scoreboard): columnas separadas en sources distintos para mayor control visual */
/* Mode 3 (Scoreboard): separate columns in different sources for finer visual control. */
	obs_source_t *sb_pos_source;
	obs_source_t *sb_name_source;
	obs_source_t *sb_solved_source;
	obs_source_t *sb_time_source;
	float scoreboard_offset_x;
	float scoreboard_offset_y;
	bool scoreboard_centered;
	int scoreboard_font_size;
	char *scoreboard_font_face;
	uint32_t scoreboard_text_color;
	uint32_t scoreboard_outline_color;
	int scoreboard_outline_size;

/* Aesthetic background for the text overlay (Scoreboard/Team Info). */
	/* Fondo estetico para el overlay de texto (Scoreboard/Team Info) */
	bool overlay_bg_enabled;
	uint32_t overlay_bg_color; /* 0xRRGGBB */
	int overlay_bg_opacity;    /* 0..100 */
	int overlay_bg_padding;    /* px (en el espacio del texto; se escala junto con el overlay AR) */
	int overlay_bg_radius;     /* px (esquinas redondeadas) */
	bool overlay_bg_shadow_enabled;
	int overlay_bg_shadow_opacity; /* 0..100 */
	int overlay_bg_shadow_offset_x; /* px */
	int overlay_bg_shadow_offset_y; /* px */
	int overlay_bg_shadow_softness; /* 1..8 capas */

	/* Suavizado AR del overlay (modo 3 y 4) para reducir jitter */
/* AR smoothing for the overlay (modes 3 and 4) to reduce jitter. */
	bool overlay_ar_smooth_enabled;
	float overlay_ar_smooth_alpha; /* 0..1 */
	bool overlay_ar_smooth_valid;
	int overlay_ar_smooth_marker_id;
	float overlay_ar_smooth_x;
	float overlay_ar_smooth_y;
	float overlay_ar_smooth_scale;
	float overlay_ar_smooth_scale_x;
	float overlay_ar_smooth_scale_y;
	float overlay_ar_smooth_angle;

	/* Tabla Scoreboard (modo 3): formato profesional */
/* Scoreboard table (mode 3): professional layout. */
	int scoreboard_name_max_chars;     /* Truncado visual con ... (UTF-8 safe) */
	bool scoreboard_row_stripes;       /* Bandas por fila */
	int scoreboard_row_stripe_opacity; /* 0..80 */
	int scoreboard_line_count;         /* Numero de lineas del texto */
	int scoreboard_header_lines;       /* Lineas de cabecera */
	int scoreboard_row_count;          /* Filas renderizadas */

	int clock_mode;              // 0 = tres manecillas, 1 = una manecilla
	int mesh_id_dial;            //
	int mesh_id_hour_hand;       // ID de la malla de la manecilla de horas
	int mesh_id_minute_hand;     // ID 
	int mesh_id_second_hand;     // I
	int mesh_id_single_hand;     // ID
	bool countdown_use_ar;       // true = usar AR para posicionar reloj, false = posicion manual

/* Team Info (mode 4): ArUco ID -> team_id mapping loaded from local JSON. */
	/* Team Info (modo 4): mapeo ArUco ID -> team_id cargado desde JSON local */
	char *team_info_json_path; /* Ruta al JSON (guardada en settings) */
	struct team_info_mapping *team_info_mappings;
	size_t team_info_mappings_count;
/* List of allowed IDs (derived from the JSON) for multi-marker detection. */
	/* Lista de IDs permitidos (derivada del JSON) para detección multi-marcador */
	int *team_info_allowed_marker_ids;
	size_t team_info_allowed_marker_ids_count;
	int team_info_detected_marker;    // Marker ID actualmente detectado (-1 = ninguno)
	scoreboard_team_t team_info_cache[100];  // Cache local de equipos
	int team_info_cache_count;
};

/* Releases all Team Info mapping buffers and resets their counters. */
/* Libera todos los buffers de mapeo de Team Info y reinicia sus contadores. */
static void team_info_clear_mappings(struct cube_filter_data *filter)
{
	if (!filter)
		return;

	if (filter->team_info_mappings) {
		bfree(filter->team_info_mappings);
		filter->team_info_mappings = NULL;
	}
	filter->team_info_mappings_count = 0;

	if (filter->team_info_allowed_marker_ids) {
		bfree(filter->team_info_allowed_marker_ids);
		filter->team_info_allowed_marker_ids = NULL;
	}
	filter->team_info_allowed_marker_ids_count = 0;
}

/* Creates the base vertex buffer used for the overlay background. */
/* Crea el buffer de vertices base usado para el fondo del overlay. */
static void overlay_bg_init_graphics(struct cube_filter_data *filter)
{
	if (!filter)
		return;

	if (filter->overlay_bg_vb)
		return;

	/* Creamos un quad unidad en el plano XY. Se escala/traslada con matrices en render. */
/* We create a unit quad on the XY plane. It is scaled and translated with matrices in render. */
	obs_enter_graphics();
	gs_render_start(true);

	gs_vertex3f(0.0f, 0.0f, 0.0f);
	gs_vertex3f(1.0f, 0.0f, 0.0f);
	gs_vertex3f(0.0f, 1.0f, 0.0f);

	gs_vertex3f(0.0f, 1.0f, 0.0f);
	gs_vertex3f(1.0f, 1.0f, 0.0f);
	gs_vertex3f(1.0f, 0.0f, 0.0f);

	filter->overlay_bg_vb = gs_render_save();
	obs_leave_graphics();
}

/* Destroys the rounded overlay background geometry if it exists. */
/* Destruye la geometria redondeada del fondo del overlay si existe. */
static void overlay_bg_free_round_graphics(struct cube_filter_data *filter)
{
	if (!filter || !filter->overlay_bg_round_vb)
		return;

	obs_enter_graphics();
	gs_vertexbuffer_destroy(filter->overlay_bg_round_vb);
	obs_leave_graphics();

	filter->overlay_bg_round_vb = NULL;
}

/* Releases all overlay background GPU resources. */
/* Libera todos los recursos GPU del fondo del overlay. */
static void overlay_bg_free_graphics(struct cube_filter_data *filter)
{
	if (!filter)
		return;

	overlay_bg_free_round_graphics(filter);

	if (filter->overlay_bg_vb) {
		obs_enter_graphics();
		gs_vertexbuffer_destroy(filter->overlay_bg_vb);
		obs_leave_graphics();

		filter->overlay_bg_vb = NULL;
	}

	filter->overlay_bg_round_last_w = 0.0f;
	filter->overlay_bg_round_last_h = 0.0f;
	filter->overlay_bg_round_last_radius = 0;
}

/* Returns the shortest signed angular difference in radians. */
/* Devuelve la diferencia angular firmada mas corta en radianes. */
static float wrap_angle_delta(float current, float target)
{
	/* Devuelve el delta mas corto en radianes, en el rango [-pi, pi]. */
/* Returns the shortest delta in radians, in the range [-pi, pi]. */
	float d = target - current;
	while (d > (float)M_PI) d -= 2.0f * (float)M_PI;
	while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
	return d;
}

/* Regenerates rounded overlay geometry when the dimensions change. */
/* Regenera la geometria redondeada del overlay cuando cambian las dimensiones. */
static void overlay_bg_update_rounded_geometry(struct cube_filter_data *filter,
					      float w, float h, int radius_px)
{
	if (!filter)
		return;

	/* Si el radio es 0, no necesitamos geometria redondeada. */
/* If the radius is 0, rounded geometry is not needed. */
	if (radius_px <= 0) {
		overlay_bg_free_round_graphics(filter);
		filter->overlay_bg_round_last_w = 0.0f;
		filter->overlay_bg_round_last_h = 0.0f;
		filter->overlay_bg_round_last_radius = 0;
		return;
	}

	/* Evitar regenerar continuamente por cambios minimos (por ejemplo, oscilaciones de 0.001). */
/* Avoid regenerating continuously for tiny changes (for example, 0.001 oscillations). */
	const float dw = fabsf(w - filter->overlay_bg_round_last_w);
	const float dh = fabsf(h - filter->overlay_bg_round_last_h);
	if (filter->overlay_bg_round_vb && dw < 0.5f && dh < 0.5f &&
	    radius_px == filter->overlay_bg_round_last_radius) {
		return;
	}

	/* Clamp defensivo del radio. */
/* Defensive clamp for the radius. */
	int r = radius_px;
	const float max_r = fminf(w, h) * 0.5f;
	if ((float)r > max_r)
		r = (int)max_r;
	if (r < 1)
		r = 1;

	/* Regenerar geometria: poligono convexo con esquinas redondeadas triangulado como fan.
	 * Nota: se usa un numero fijo de segmentos para no cargar el hilo de render.
	 */
	enum { OVERLAY_BG_ROUND_SEGMENTS = 8 };
	const int arc_pts = OVERLAY_BG_ROUND_SEGMENTS + 1; /* incluye extremos */
	struct pt2 { float x, y; } pts[64];
	int n = 0;

	const float rf = (float)r;

	/* Construir contorno en sentido horario, asumiendo coord 2D de pantalla (y hacia abajo). */
/* Build the contour clockwise, assuming screen-space 2D coordinates (Y downward). */
	#define ADD_PT(_x, _y) do { \
		const float __x = (_x); \
		const float __y = (_y); \
		if (n > 0) { \
			if (fabsf(pts[n - 1].x - __x) < 0.001f && fabsf(pts[n - 1].y - __y) < 0.001f) \
				break; \
		} \
		if (n < (int)(sizeof(pts) / sizeof(pts[0]))) { \
			pts[n].x = __x; \
			pts[n].y = __y; \
			n++; \
		} \
	} while (0)

	ADD_PT(rf, 0.0f);
	ADD_PT(w - rf, 0.0f);
	/* Esquina sup-der (centro = w-r, r), angulos -90..0 */
/* Top-right corner (center = w-r, r), angles -90..0. */
	for (int i = 0; i < arc_pts; i++) {
		const float a = (-0.5f * (float)M_PI) + (0.5f * (float)M_PI) * ((float)i / (float)(arc_pts - 1));
		ADD_PT((w - rf) + cosf(a) * rf, (rf) + sinf(a) * rf);
	}
	ADD_PT(w, rf);
	ADD_PT(w, h - rf);
	/* Esquina inf-der (centro = w-r, h-r), angulos 0..90 */
/* Bottom-right corner (center = w-r, h-r), angles 0..90. */
	for (int i = 0; i < arc_pts; i++) {
		const float a = 0.0f + (0.5f * (float)M_PI) * ((float)i / (float)(arc_pts - 1));
		ADD_PT((w - rf) + cosf(a) * rf, (h - rf) + sinf(a) * rf);
	}
	ADD_PT(w - rf, h);
	ADD_PT(rf, h);
	/* Esquina inf-izq (centro = r, h-r), angulos 90..180 */
/* Bottom-left corner (center = r, h-r), angles 90..180. */
	for (int i = 0; i < arc_pts; i++) {
		const float a = (0.5f * (float)M_PI) + (0.5f * (float)M_PI) * ((float)i / (float)(arc_pts - 1));
		ADD_PT((rf) + cosf(a) * rf, (h - rf) + sinf(a) * rf);
	}
	ADD_PT(0.0f, h - rf);
	ADD_PT(0.0f, rf);
	/* Esquina sup-izq (centro = r, r), angulos 180..270 */
/* Top-left corner (center = r, r), angles 180..270. */
	for (int i = 0; i < arc_pts; i++) {
		const float a = (float)M_PI + (0.5f * (float)M_PI) * ((float)i / (float)(arc_pts - 1));
		ADD_PT((rf) + cosf(a) * rf, (rf) + sinf(a) * rf);
	}
	#undef ADD_PT

	if (n < 3)
		return;

	/* Destruir anterior y crear nuevo */
/* Destroy the previous geometry and create a new one. */
	overlay_bg_free_round_graphics(filter);

	obs_enter_graphics();
	gs_render_start(true);
	const float cx = w * 0.5f;
	const float cy = h * 0.5f;
	for (int i = 0; i < n; i++) {
		const int j = (i + 1) % n;
		gs_vertex3f(cx, cy, 0.0f);
		gs_vertex3f(pts[i].x, pts[i].y, 0.0f);
		gs_vertex3f(pts[j].x, pts[j].y, 0.0f);
	}
	filter->overlay_bg_round_vb = gs_render_save();
	obs_leave_graphics();

	filter->overlay_bg_round_last_w = w;
	filter->overlay_bg_round_last_h = h;
	filter->overlay_bg_round_last_radius = radius_px;
}

/* Looks up the team identifier assigned to an ArUco marker. */
/* Busca el identificador de equipo asignado a un marcador ArUco. */
static const char *team_info_lookup_team_id(struct cube_filter_data *filter,
					    int aruco_id)
{
	if (!filter || !filter->team_info_mappings ||
	    filter->team_info_mappings_count == 0)
		return NULL;

	for (size_t i = 0; i < filter->team_info_mappings_count; i++) {
		if (filter->team_info_mappings[i].aruco_id == aruco_id)
			return filter->team_info_mappings[i].team_id;
	}
	return NULL;
}

/* Parses Team Info mappings from JSON into internal storage. */
/* Analiza los mapeos de Team Info desde JSON hacia el almacenamiento interno. */
static bool team_info_parse_json_mappings(const char *json,
					 struct team_info_mapping **out_mappings,
					 size_t *out_count)
{
	if (!json || !out_mappings || !out_count)
		return false;

	*out_mappings = NULL;
	*out_count = 0;

	size_t capacity = 16;
	struct team_info_mapping *mappings =
		bmalloc(sizeof(*mappings) * capacity);
	if (!mappings)
		return false;

	size_t count = 0;
	size_t duplicated = 0;

	/* Formatos soportados:
	 * A) Array de objetos (recomendado):
	 *    [ { "marker_id": 12, "team_id": "42" }, ... ]
	 *    (también se acepta "aruco_id" por compatibilidad)
	 *    o { "mappings": [ ... ] }
	 * B) Objeto mapa (clave = ID ArUco):
	 *    { "12": "42", "13": "team_abc" }
	 */
	const char *arr_start = strchr(json, '[');
	if (arr_start) {
		const char *p = arr_start + 1;
		while (true) {
			const char *obj = find_next_json_object(p);
			if (!obj)
				break;
			const char *obj_end = find_closing_brace(obj);
			if (!obj_end)
				break;

			size_t obj_len = (size_t)(obj_end - obj + 1);
/* Defensive limit to avoid huge allocations from corrupted JSON. */
			/* LÃ­mite defensivo para evitar allocs enormes por JSON corrupto */
			if (obj_len > 8192) {
				p = obj_end + 1;
				continue;
			}

			char *temp = bmalloc(obj_len + 1);
			if (!temp)
				break;
			memcpy(temp, obj, obj_len);
			temp[obj_len] = '\0';

			uint32_t aruco_id_u = 0;
			bool got_aruco =
				parse_json_int(temp, "aruco_id", &aruco_id_u) ||
				parse_json_int(temp, "arucoId", &aruco_id_u) ||
				parse_json_int(temp, "marker_id", &aruco_id_u) ||
				parse_json_int(temp, "markerId", &aruco_id_u);

			char team_id[64] = {0};
			bool got_team =
				parse_json_string(temp, "team_id", team_id,
						  sizeof(team_id)) ||
				parse_json_string(temp, "teamId", team_id,
						  sizeof(team_id)) ||
				parse_json_string(temp, "team", team_id,
						  sizeof(team_id));

			if (got_aruco && got_team && team_id[0]) {
				int aruco_id = (int)aruco_id_u;

/* If aruco_id already exists, overwrite it (the last one is used). */
				/* Si ya existe el aruco_id, sobrescribe (se usa el Ãºltimo) */
				bool replaced = false;
				for (size_t i = 0; i < count; i++) {
					if (mappings[i].aruco_id == aruco_id) {
						strncpy(mappings[i].team_id, team_id,
							sizeof(mappings[i].team_id) - 1);
						mappings[i].team_id[sizeof(mappings[i].team_id) - 1] =
							'\0';
						duplicated++;
						replaced = true;
						break;
					}
				}

				if (!replaced) {
					if (count == capacity) {
						capacity *= 2;
						struct team_info_mapping *new_map =
							brealloc(mappings,
								 sizeof(*mappings) *
									 capacity);
						if (!new_map) {
							bfree(temp);
							break;
						}
						mappings = new_map;
					}

					mappings[count].aruco_id = aruco_id;
					strncpy(mappings[count].team_id, team_id,
						sizeof(mappings[count].team_id) - 1);
					mappings[count].team_id[sizeof(mappings[count].team_id) - 1] =
						'\0';
					count++;
				}
			}

			bfree(temp);
			p = obj_end + 1;
		}
	} else {
		/* Parser simple para objeto mapa: { "12": "42", ... } */
/* Simple parser for map objects: { "12": "42", ... }. */
		const char *p = strchr(json, '{');
		if (p) {
			p++; /* saltar '{' */
			while (*p) {
				while (*p == ' ' || *p == '\t' || *p == '\n' ||
				       *p == '\r' || *p == ',')
					p++;
				if (*p == '}')
					break;

				if (*p != '\"')
					break;
				p++; /* comilla apertura clave */

				char key_buf[32] = {0};
				size_t ki = 0;
				while (*p && *p != '\"' && ki < sizeof(key_buf) - 1) {
					key_buf[ki++] = *p++;
				}
				key_buf[ki] = '\0';
				if (*p != '\"')
					break;
				p++; /* comilla cierre clave */

				while (*p == ' ' || *p == '\t' || *p == '\n' ||
				       *p == '\r')
					p++;
				if (*p != ':')
					break;
				p++; /* ':' */
				while (*p == ' ' || *p == '\t' || *p == '\n' ||
				       *p == '\r')
					p++;

				char *endptr = NULL;
				long aruco_id_l = strtol(key_buf, &endptr, 10);
				bool aruco_id_valid = (endptr && *endptr == '\0' &&
						       aruco_id_l >= 0 &&
						       aruco_id_l <= INT_MAX);
				int aruco_id = (int)aruco_id_l;

				char team_id[64] = {0};
				if (*p == '\"') {
					p++; /* comilla apertura valor */
					size_t vi = 0;
					while (*p && *p != '\"' &&
					       vi < sizeof(team_id) - 1) {
						if (*p == '\\' && *(p + 1)) {
							p++; /* saltar escape */
						}
						team_id[vi++] = *p++;
					}
					team_id[vi] = '\0';
					if (*p == '\"')
						p++;
				} else if (*p >= '0' && *p <= '9') {
/* Allow numeric values. */
					/* Permitir valores numÃ©ricos */
					size_t vi = 0;
					while (*p >= '0' && *p <= '9' &&
					       vi < sizeof(team_id) - 1) {
						team_id[vi++] = *p++;
					}
					team_id[vi] = '\0';
				} else {
					/* valor no soportado */
/* unsupported value. */
					break;
				}

				if (team_id[0] && aruco_id_valid) {
					bool replaced = false;
					for (size_t i = 0; i < count; i++) {
						if (mappings[i].aruco_id == aruco_id) {
							strncpy(mappings[i].team_id, team_id,
								sizeof(mappings[i].team_id) - 1);
							mappings[i].team_id[sizeof(mappings[i].team_id) - 1] =
								'\0';
							duplicated++;
							replaced = true;
							break;
						}
					}

					if (!replaced) {
						if (count == capacity) {
							capacity *= 2;
							struct team_info_mapping *new_map =
								brealloc(mappings,
									 sizeof(*mappings) *
										 capacity);
							if (!new_map)
								break;
							mappings = new_map;
						}
						mappings[count].aruco_id = aruco_id;
						strncpy(mappings[count].team_id, team_id,
							sizeof(mappings[count].team_id) - 1);
						mappings[count].team_id[sizeof(mappings[count].team_id) - 1] =
							'\0';
						count++;
					}
				}
			}
		}
	}

	if (count == 0) {
		bfree(mappings);
		return false;
	}

	if (duplicated > 0) {
		blog(LOG_INFO,
		     "[CUBE-TEAM-INFO] IDs ArUco duplicados en JSON: %zu (se usa el ultimo)",
		     duplicated);
	}

	*out_mappings = mappings;
	*out_count = count;
	return true;
}

/* Loads Team Info mappings from a JSON file on disk. */
/* Carga los mapeos de Team Info desde un archivo JSON en disco. */
static bool team_info_load_json_from_path(struct cube_filter_data *filter,
					 const char *path)
{
	if (!filter)
		return false;

	if (!path || !path[0]) {
		blog(LOG_INFO,
		     "[CUBE-TEAM-INFO] Ruta de JSON local vacia: se limpian mapeos");
		team_info_clear_mappings(filter);
		return true;
	}

	FILE *f = os_fopen(path, "rb");
	if (!f) {
		blog(LOG_WARNING,
		     "[CUBE-TEAM-INFO] No se pudo abrir el JSON local: %s",
		     path);
		return false;
	}

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		blog(LOG_WARNING,
		     "[CUBE-TEAM-INFO] No se pudo obtener el tamano del JSON: %s",
		     path);
		return false;
	}

	long file_size = ftell(f);
	if (file_size < 0) {
		fclose(f);
		blog(LOG_WARNING,
		     "[CUBE-TEAM-INFO] Tamano de JSON invalido: %s", path);
		return false;
	}
	rewind(f);

/* Defensive limit: the JSON should be small. */
	/* LÃ­mite defensivo: el JSON deberÃ­a ser pequeÃ±o */
	if (file_size > (1024 * 1024)) {
		fclose(f);
	blog(LOG_WARNING,
	     "[CUBE-TEAM-INFO] JSON demasiado grande (%ld bytes). Maximo 1MB: %s",
	     file_size, path);
		return false;
	}

	char *json = bmalloc((size_t)file_size + 1);
	if (!json) {
		fclose(f);
		blog(LOG_WARNING,
		     "[CUBE-TEAM-INFO] Sin memoria para leer el JSON (%ld bytes)",
		     file_size);
		return false;
	}

	size_t read_bytes = fread(json, 1, (size_t)file_size, f);
	fclose(f);
	json[read_bytes] = '\0';

	struct team_info_mapping *new_mappings = NULL;
	size_t new_count = 0;
	bool ok = team_info_parse_json_mappings(json, &new_mappings, &new_count);
	bfree(json);

	if (!ok) {
		blog(LOG_WARNING,
		     "[CUBE-TEAM-INFO] Fallo el parseo del JSON local: %s", path);
		return false;
	}

	team_info_clear_mappings(filter);
	filter->team_info_mappings = new_mappings;
	filter->team_info_mappings_count = new_count;

/* Build the list of allowed IDs for multi-marker detection (Team Info). */
	/* Construir lista de IDs permitidos para detección multi-marcador (Team Info) */
	if (new_count > 0) {
		int *allowed = bzalloc(sizeof(int) * new_count);
		size_t allowed_count = 0;

		for (size_t i = 0; i < new_count; i++) {
			const int id = new_mappings[i].aruco_id;
			bool exists = false;
			for (size_t j = 0; j < allowed_count; j++) {
				if (allowed[j] == id) {
					exists = true;
					break;
				}
			}
			if (!exists)
				allowed[allowed_count++] = id;
		}

		if (allowed_count == 0) {
			bfree(allowed);
		} else {
			filter->team_info_allowed_marker_ids = allowed;
			filter->team_info_allowed_marker_ids_count = allowed_count;
			blog(LOG_INFO,
			     "[CUBE-TEAM-INFO] IDs permitidos cargados desde JSON: %zu",
			     allowed_count);
		}
	}

	blog(LOG_INFO, "[CUBE-TEAM-INFO] JSON local cargado: %zu mapeos (%s)",
	     new_count, path);
	return true;
}

/* Processes each video frame and updates AR detection state. */
/* Procesa cada fotograma de video y actualiza el estado de deteccion AR. */
static struct obs_source_frame *filter_video(void *data,
					     struct obs_source_frame *frame)
{
	struct cube_filter_data *filter = data;

	if (!frame) {
		blog(LOG_WARNING, "cube_filter_filter_video: frame es NULL");
		return NULL;
	}

	if (filter->mode == 0 || (filter->mode == 2 && !filter->countdown_use_ar)) {
		filter->current_scale = filter->scale;
		filter->last_result.detected = false;
		return frame;
	}

	bool detected = false;

	/* Importante: usar el MISMO espacio de coordenadas que el render 2D (gs_ortho).
	 * Esto evita offsets extraños (por ejemplo, que el texto se vaya a una esquina del marcador)
	 * cuando la resolución del frame no coincide con la resolución base de OBS.
	 */
		const int base_w = (int)obs_source_get_width(filter->source);
		const int base_h = (int)obs_source_get_height(filter->source);
		const int out_w = base_w;
		const int out_h = base_h;

	if (filter->mode == 4 && filter->team_info_allowed_marker_ids_count > 0) {
		detected = process_frame_rgba_select_ids(
			filter->detector, frame,
			base_w, base_h, out_w, out_h,
			filter->team_info_allowed_marker_ids,
			filter->team_info_allowed_marker_ids_count,
			&filter->last_result);
	} else {
		detected = process_frame_rgba(filter->detector, frame,
					      base_w, base_h, out_w, out_h,
					      &filter->last_result);
	}

	if (detected && filter->last_result.detected) {
		
			static int log_throttle_marker = 0;
			if ((log_throttle_marker++ % 90) == 0) {
				blog(LOG_INFO, "[CUBE-AR] Detectado marcador ID: %d",
				     filter->last_result.id);
			}

/* Team Info: store the detected ID for the lookup (mode 4). */
			/* Team Info: guardar el ID detectado para el lookup (modo 4) */
			if (filter->mode == 4) {
				filter->team_info_detected_marker = filter->last_result.id;
			}
	
		filter->pos_x = filter->last_result.screen_pos_x +filter->ar_offset_pos_x;
		/* Invertir Y para que coincida con el sistema de OBS (0 abajo) */
/* Invert Y so it matches the OBS coordinate system (0 at the bottom). */
		filter->pos_y =
			(float)base_h - (filter->last_result.screen_pos_y +
					 filter->ar_offset_pos_y);
		;
		filter->pos_z =0 +filter->ar_offset_pos_z; 

		/* Escala por distancia (tvec[2]): cuanto mas cerca el marcador, mas grande el objeto */
		const float reference_distance = 1.0f;
		const float z = filter->last_result.tvec[2];
		if (z > 0.05f) {
			float distance_scale = reference_distance / z;
			distance_scale = clampf(distance_scale, 0.2f, 4.0f);
			filter->current_scale = filter->scale * distance_scale;
		} else {
			filter->current_scale = filter->scale;
		}

	} else {
		filter->last_result.detected = false;
		if (filter->mode == 4)
			filter->team_info_detected_marker = -1;
	}

	return frame;
}

/* Returns the current source width in pixels. */
/* Devuelve el ancho actual de la fuente en pixeles. */
static uint32_t cube_source_get_width(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->width_screen;
}

/* Returns the current source height in pixels. */
/* Devuelve la altura actual de la fuente en pixeles. */
static uint32_t cube_source_get_height(void *data)
{
	struct cube_filter_data *filter = data;
	return filter->height_screen;
}
/* Loads a texture from disk and returns the GPU texture handle. */
/* Carga una textura desde disco y devuelve el manejador de la textura GPU. */
gs_texture_t *load_texture_file(const char *path)
{
	if (!path || strlen(path) == 0) {
		return NULL;
	}

	obs_enter_graphics();
	gs_image_file_t image;
	gs_image_file_init(&image, path);
	gs_image_file_init_texture(&image);
	obs_leave_graphics();

	if (image.loaded) {
		blog(LOG_INFO, "Textura de usuario cargada: %s", path);
		gs_texture_t *new_texture = image.texture;
		image.texture = NULL; // Evitar que free lo destruya si lo estamos devolviendo (depende de la implemenation de OBS, pero por si acaso)
		gs_image_file_free(&image);
		return new_texture;
	} else {
		blog(LOG_WARNING, "No se pudo cargar la textura de usuario: %s",
		     path);
		gs_image_file_free(&image); // Asegúrate de liberar la estructura de imagen
		return NULL;
	}
}
/* Reloads an OBS image file and rebuilds its texture. */
/* Recarga un archivo de imagen de OBS y reconstruye su textura. */
void image_source_load(gs_image_file_t *image, const char *file)
{
	obs_enter_graphics();
	gs_image_file_free(image);
	obs_leave_graphics();

	gs_image_file_init(image, file);

	obs_enter_graphics();
	gs_image_file_init_texture(image);
	obs_leave_graphics();

	if (!image->loaded) {
		blog(LOG_WARNING, "failed to load texture %s", file);
	}
}

/* Creates or recreates the render textures used by the filter. */
/* Crea o recrea las texturas de render usadas por el filtro. */
void create_texture(struct cube_filter_data *data)
{
	obs_enter_graphics();

	if (data->texture != NULL) {
		gs_texture_destroy(data->texture);
		data->texture = NULL;
	}

	data->texture = gs_texture_create(data->width_screen,
					  data->height_screen, GS_RGBA, 1, NULL,
					  GS_RENDER_TARGET);
	data->zstencil = gs_zstencil_create(data->width_screen,
					    data->height_screen, GS_Z16);
	blog(LOG_INFO, "create whiteboard texture %d %d", data->width_screen,
	     data->height_screen);

	obs_leave_graphics();
}

/* Returns the user-facing filter name shown by OBS. */
/* Devuelve el nombre del filtro visible para el usuario en OBS. */
static const char *filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "SJ_3D";
}

/* Allocates and initializes the filter instance state. */
/* Reserva e inicializa el estado de la instancia del filtro. */
static void *filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct cube_filter_data *data =
		bzalloc(sizeof(struct cube_filter_data));

	data->source = source;
	data->g_meshes = NULL;
	data->g_mesh_count = 0;
	data->model_width = NULL;  //
	data->model_height = NULL; //
	data->loaded_texture = NULL;
	data->overlay_bg_vb = NULL;
	data->overlay_bg_round_vb = NULL;
	data->overlay_bg_round_last_w = 0.0f;
	data->overlay_bg_round_last_h = 0.0f;
	data->overlay_bg_round_last_radius = 0;

	data->detector =
		initialize_aruco_detector(0.1f, ARUCO_DICT_ORIGINAL, NULL);

	data->countdown_clock = countdown_clock_create();
	data->web_sync = NULL;
	data->countdown_running = false;
	data->countdown_reset_requested = false;
	data->countdown_duration_h = 0;
	data->countdown_duration_m = 0;
	data->countdown_duration_s = 0;
	data->sync_enabled = false;
	data->sync_interval_sec = 10.0f;

	data->api_base_url = NULL;
	data->contest_id = NULL;
	data->api_username = NULL;
	data->api_password = NULL;
	data->scoreboard_team_count = 0;
	data->scoreboard_text_source = NULL;
	data->sb_pos_source = NULL;
	data->sb_name_source = NULL;
	data->sb_solved_source = NULL;
	data->sb_time_source = NULL;
	data->scoreboard_offset_x = 10.0f;
	data->scoreboard_offset_y = 10.0f;
	data->scoreboard_centered = false;
	data->scoreboard_font_size = 25;
	data->scoreboard_font_face = bstrdup("Arial");
	data->scoreboard_text_color = 0xFFFFFFFF;    // Blanco por defecto
	data->scoreboard_outline_color = 0xFF000000; // Negro por defecto
	data->scoreboard_outline_size = 2;

/* Default overlay background (dark and semi-transparent). */
	/* Fondo del overlay por defecto (oscuro y semi-transparente) */
	data->overlay_bg_enabled = true;
	data->overlay_bg_color = 0x101010;
	data->overlay_bg_opacity = 70;
	data->overlay_bg_padding = 12;
	data->overlay_bg_radius = 14;
	data->overlay_bg_shadow_enabled = true;
	data->overlay_bg_shadow_opacity = 35;
	data->overlay_bg_shadow_offset_x = 4;
	data->overlay_bg_shadow_offset_y = 4;
	data->overlay_bg_shadow_softness = 4;

	data->overlay_ar_smooth_enabled = true;
	data->overlay_ar_smooth_alpha = 0.20f;
	data->overlay_ar_smooth_valid = false;
	data->overlay_ar_smooth_marker_id = -1;
	data->overlay_ar_smooth_x = 0.0f;
	data->overlay_ar_smooth_y = 0.0f;
	data->overlay_ar_smooth_scale = 1.0f;
	data->overlay_ar_smooth_scale_x = 1.0f;
	data->overlay_ar_smooth_scale_y = 1.0f;
	data->overlay_ar_smooth_angle = 0.0f;

	/* Scoreboard tabla: defaults */
	data->scoreboard_name_max_chars = 20;
	data->scoreboard_row_stripes = true;
	data->scoreboard_row_stripe_opacity = 18;
	data->scoreboard_line_count = 0;
	data->scoreboard_header_lines = 2;
	data->scoreboard_row_count = 0;

	data->clock_mode = 0;              // Tres manecillas por defecto
	data->mesh_id_dial = 0;
	data->mesh_id_hour_hand = 1;
	data->mesh_id_minute_hand = 2;
	data->mesh_id_second_hand = 3;
	data->mesh_id_single_hand = 1;
	data->countdown_use_ar = false;    // Posicion manual por defecto

	/* Team Info: JSON local (mapeos ArUco->team) */
	data->team_info_json_path = NULL;
	data->team_info_mappings = NULL;
	data->team_info_mappings_count = 0;
	data->team_info_allowed_marker_ids = NULL;
	data->team_info_allowed_marker_ids_count = 0;
	data->team_info_detected_marker = -1;
	data->team_info_cache_count = 0;

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		data->width_screen = ovi.base_width;
		data->height_screen = ovi.base_height;
		create_texture(data);
		overlay_bg_init_graphics(data);
	} else {
		blog(LOG_WARNING, "Failed to get video resolution");
	}

	return data;
}

/* Frees all resources owned by the filter instance. */
/* Libera todos los recursos propiedad de la instancia del filtro. */
static void filter_destroy(void *data)
{
	/*blog(LOG_WARNING, "CERRANDO");*/
	struct cube_filter_data *filter = (struct cube_filter_data *)data;
	cleanup_global_meshes(&filter->g_meshes, &filter->g_mesh_count,
			      &filter->model_width, &filter->model_height,
			      filter->loaded_texture);

	/* Liberar recursos de GPU del fondo del overlay (no anidar obs_enter_graphics) */
	overlay_bg_free_graphics(filter);

	/* Liberar el detector ArUco (evita fugas de memoria y recursos OpenCV) */
	if (filter->detector) {
		cleanup_aruco_detector(filter->detector);
		filter->detector = NULL;
	}
	obs_enter_graphics();
	if (filter->texture) {
		gs_texture_destroy(filter->texture);
	}
	if (filter->loaded_texture) {
		gs_texture_destroy(filter->loaded_texture);
	}
	if (filter->zstencil) {
		gs_zstencil_destroy(filter->zstencil);
	}

	obs_leave_graphics();
	if (filter->countdown_clock)countdown_clock_destroy(filter->countdown_clock);
	if (filter->web_sync)web_sync_destroy(filter->web_sync);
	if (filter->scoreboard_text_source) {
		obs_source_release(filter->scoreboard_text_source);
		filter->scoreboard_text_source = NULL;
	}
	if (filter->sb_pos_source) {
		obs_source_release(filter->sb_pos_source);
		filter->sb_pos_source = NULL;
	}
	if (filter->sb_name_source) {
		obs_source_release(filter->sb_name_source);
		filter->sb_name_source = NULL;
	}
	if (filter->sb_solved_source) {
		obs_source_release(filter->sb_solved_source);
		filter->sb_solved_source = NULL;
	}
	if (filter->sb_time_source) {
		obs_source_release(filter->sb_time_source);
		filter->sb_time_source = NULL;
	}
	bfree(filter->api_base_url);
	bfree(filter->contest_id);
	bfree(filter->api_username);
	bfree(filter->api_password);
	bfree(filter->model_path_str);
	bfree(filter->texture_path_str);
	bfree(filter->scoreboard_font_face);
	bfree(filter->team_info_json_path);
	team_info_clear_mappings(filter);
	bfree(filter);
}

/* Renders the filter output for the active mode. */
/* Renderiza la salida del filtro para el modo activo. */
static void filter_render(void *data, gs_effect_t *effect)
{
	struct cube_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->source);
	if (!target) {
		return;
	}

	uint32_t width = obs_source_get_width(filter->source);
	uint32_t height = obs_source_get_height(filter->source);

	
	if (width == 0 || height == 0 || (filter->mode != 3 && filter->mode != 4 && !filter->g_meshes)) {
		obs_source_skip_video_filter(filter->source);
		return;
	}
	if (filter->mode != 3 && (filter->mode == 1 || (filter->mode == 2 && filter->countdown_use_ar)) && !filter->last_result.detected) {
		obs_source_skip_video_filter(filter->source);
		return;
	}
	if (filter->mode == 2 && (!filter->g_meshes || filter->g_mesh_count == 0)) {
		obs_source_skip_video_filter(filter->source);
		return;
	}
	
	if (filter->mode == 4 && !filter->scoreboard_text_source) {
		obs_source_skip_video_filter(filter->source);
		return;
	}
	if (filter->mode == 3 &&
	    (!filter->sb_pos_source || !filter->sb_name_source ||
	     !filter->sb_solved_source || !filter->sb_time_source)) {
		obs_source_skip_video_filter(filter->source);
		return;
	}
	obs_enter_graphics();
	if (!filter->texture ||
	    gs_texture_get_width(filter->texture) != width ||
	    gs_texture_get_height(filter->texture) != height) {
		if (filter->texture)
			gs_texture_destroy(filter->texture);
		if (filter->zstencil)
			gs_zstencil_destroy(filter->zstencil);

		filter->texture = gs_texture_create(width, height, GS_RGBA, 1,
						    NULL, GS_RENDER_TARGET);
		filter->zstencil = gs_zstencil_create(width, height, GS_Z16);
	}


	if (filter->mode != 3 && filter->g_meshes) {
		gs_texture_t *prev_render_target = gs_get_render_target();
		gs_zstencil_t *prev_zstencil_target = gs_get_zstencil_target();

		gs_set_render_target(filter->texture, filter->zstencil);
		gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH, (struct vec4[]){{0.0f, 0.0f, 0.0f, 0.0f}}, 1.0f, 0);

		gs_projection_push();
		gs_set_3d_mode(60.0f, 0.1f, 5000.0f); 
		gs_enable_depth_test(true);
		gs_depth_function(GS_LESS);

		gs_matrix_push();
		gs_matrix_identity();

		gs_matrix_translate3f(filter->pos_x, filter->pos_y, filter->pos_z);


		float rot_x, rot_y, rot_z;
		if (filter->mode == 0) {
			rot_x = filter->rotation_x;
			rot_y = filter->rotation_y;
			rot_z = filter->rotation_z;
		} else if (filter->mode == 1) {
			rot_x = filter->ar_offset_rot_x;
			rot_y = filter->ar_offset_rot_y;
			rot_z = filter->ar_offset_rot_z;
		} else {
			if (filter->countdown_use_ar) {
				rot_x = filter->ar_offset_rot_x;
				rot_y = filter->ar_offset_rot_y;
				rot_z = filter->ar_offset_rot_z;
			} else {
				rot_x = filter->rotation_x;
				rot_y = filter->rotation_y;
				rot_z = filter->rotation_z;
			}
		}

		if (filter->mode == 2 && filter->countdown_clock) {
			float hour_deg, min_deg, sec_deg, single_deg;
			countdown_clock_get_hand_angles(filter->countdown_clock,&hour_deg, &min_deg, &sec_deg);
			countdown_clock_get_single_hand_angle(filter->countdown_clock, &single_deg);
			
			render_model_clock_c(filter->g_meshes, filter->g_mesh_count,
								 filter->model_width, filter->model_height,
								 filter->current_scale, filter->last_result.rvec,
								 filter->last_result.detected,
								 rot_x, rot_y, rot_z,
								 filter->clock_mode,
								 filter->mesh_id_dial, filter->mesh_id_hour_hand,
								 filter->mesh_id_minute_hand, filter->mesh_id_second_hand,
								 filter->mesh_id_single_hand,
								 &hour_deg, &min_deg, &sec_deg, &single_deg);
		} else {
			render_model_c(filter->g_meshes, filter->g_mesh_count,
						  filter->model_width, filter->model_height,
						  filter->current_scale, filter->last_result.rvec,
						  filter->last_result.detected, rot_x, rot_y, rot_z);
		}
		gs_matrix_pop();
		gs_projection_pop();
		gs_enable_depth_test(false);

		gs_set_render_target(prev_render_target, prev_zstencil_target);
	}
	obs_leave_graphics();

	
	obs_enter_graphics();

	obs_source_video_render(target);


	if (filter->mode != 3 && filter->g_meshes) {
		gs_blend_state_push();
		gs_enable_blending(true);
		gs_blend_function(GS_BLEND_SRCALPHA,
	 GS_BLEND_INVSRCALPHA);

		gs_effect_t *base_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		while (gs_effect_loop(base_effect, "Draw")) {
			obs_source_draw(filter->texture, 0, 0, 0, 0, false);
		}

		gs_blend_state_pop();
	}

	/* Renderizar overlay de scoreboard/teaminfo si hay datos */
	if ((filter->mode == 3 &&
	     filter->sb_pos_source && filter->sb_name_source &&
	     filter->sb_solved_source && filter->sb_time_source) ||
	    (filter->mode == 4 && filter->scoreboard_text_source)) {
		uint32_t tw = 0, th = 0;
		uint32_t w_pos = 0, w_name = 0, w_res = 0, w_time = 0;
		const float col_gap = 18.0f;
		if (filter->mode == 3) {
			/* Usar ancho real de cada columna para evitar cortes de nombres y mantener RES/TIEMPO alineados. */
			w_pos = obs_source_get_width(filter->sb_pos_source);
			w_name = obs_source_get_width(filter->sb_name_source);
			w_res = obs_source_get_width(filter->sb_solved_source);
			w_time = obs_source_get_width(filter->sb_time_source);

			const uint32_t h_pos = obs_source_get_height(filter->sb_pos_source);
			const uint32_t h_name = obs_source_get_height(filter->sb_name_source);
			const uint32_t h_res = obs_source_get_height(filter->sb_solved_source);
			const uint32_t h_time = obs_source_get_height(filter->sb_time_source);

			tw = (uint32_t)((float)w_pos + col_gap + (float)w_name + col_gap +
					(float)w_res + col_gap + (float)w_time);
			th = h_pos;
			if (h_name > th) th = h_name;
			if (h_res > th) th = h_res;
			if (h_time > th) th = h_time;
		} else if (filter->mode == 3 || filter->mode == 4) {
			tw = obs_source_get_width(filter->scoreboard_text_source);
			th = obs_source_get_height(filter->scoreboard_text_source);
		}

		uint32_t cur_w = obs_source_get_width(filter->source);
		uint32_t cur_h = obs_source_get_height(filter->source);
		if (cur_w == 0) cur_w = (uint32_t)filter->width_screen;
		if (cur_h == 0) cur_h = (uint32_t)filter->height_screen;

		float x = filter->scoreboard_offset_x;
		float y = filter->scoreboard_offset_y;
		float final_x = 0.0f, final_y = 0.0f;
		float marker_edge_px = 0.0f;
		float marker_w_px = 0.0f;
		float marker_h_px = 0.0f;
		float angle_screen = 0.0f;
		float marker_cx = 0.0f;
		float marker_cy = 0.0f;

		const bool have_marker_2d =
			aruco_marker_metrics_2d(&filter->last_result,
						(float)cur_h,
						&marker_edge_px,
						&marker_w_px,
						&marker_h_px,
						&angle_screen,
						&marker_cx,
						&marker_cy);

		const bool align_overlay_to_aruco = filter->last_result.detected;
		if (align_overlay_to_aruco) {
			const float deg_to_rad = 0.017453292519943295f;
			/* Orientacion principal: proyeccion del eje Z del marcador (normal del papel).
			 * Esto mantiene el texto "recto hacia arriba" cuando el marcador esta tumbado. */
			if (filter->last_result.normal_tip_valid) {
				const float vx = filter->last_result.normal_tip_x - filter->last_result.screen_pos_x;
				const float vy = filter->last_result.normal_tip_y - filter->last_result.screen_pos_y;
				const float len = hypotf(vx, vy);
				if (len > 1e-3f) {
					float nx = vx / len;
					float ny = vy / len;

					/* Forzar que la normal proyectada apunte visualmente hacia arriba en pantalla. */
					if (ny > 0.0f) {
						nx = -nx;
						ny = -ny;
					}

					/* Convertir vector "up" a eje X del texto (perpendicular). */
					const float right_x = ny;
					const float right_y = -nx;
					angle_screen = atan2f(right_y, right_x);
				} else if (!have_marker_2d) {
					angle_screen = 0.0f;
				}
			} else if (!have_marker_2d) {
				angle_screen = 0.0f;
			}

			angle_screen += filter->ar_offset_rot_z * deg_to_rad;
		}

		if (align_overlay_to_aruco) {
			if (have_marker_2d) {
				final_x = marker_cx;
				final_y = marker_cy;
			} else {
				/* Fallback si no hay corners detectados: sistema Y-Down directo (0=arriba) */
				final_x = filter->last_result.screen_pos_x + x;
				final_y = filter->last_result.screen_pos_y + y;
			}
		} else {
			/* Posicionamiento manual o centrado en pantalla sin AR */
			if (filter->scoreboard_centered && tw > 0) {
				final_x = (float)cur_w * 0.5f;
				final_y = (float)cur_h * 0.5f;
			} else {
				final_x = x;
				final_y = y; /* Sistema 0=arriba directo */
			}
		}

		/* Configurar estado de render del overlay */
		gs_enable_depth_test(false);
		gs_blend_state_push();
		gs_enable_blending(true);
		gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
		const enum gs_cull_mode prev_cull_mode = gs_get_cull_mode();
		gs_set_cull_mode(GS_NEITHER);

		uint32_t current_source_w = obs_source_get_width(filter->source);
		uint32_t current_source_h = obs_source_get_height(filter->source);
		if (current_source_w == 0) current_source_w = (uint32_t)filter->width_screen;
		if (current_source_h == 0) current_source_h = (uint32_t)filter->height_screen;

		gs_projection_push();
		const bool overlay_use_3d_pose = align_overlay_to_aruco && filter->last_result.detected;
		if (overlay_use_3d_pose) {
			/* IMPORTANTE: gs_set_3d_mode() en libobs es un stub no implementado
			 * (libobs/graphics/graphics.c:1078 → /* TODO * con UNUSED_PARAMETER).
			 * Por eso ajustar near/far ahí no tenía efecto y el texto se recortaba
			 * en modos 3 y 4 al rotarlo (el rango Z por defecto de OBS es muy
			 * pequeño y el texto rotado se sale por el plano de corte).
			 *
			 * Usamos gs_ortho con un rango Z amplio y simétrico: las coordenadas
			 * X/Y siguen siendo píxeles (no rompe el posicionamiento del marcador)
			 * y la profundidad da margen para cualquier rotación del panel.
			 */
			gs_ortho(0.0f, (float)current_source_w,
				 0.0f, (float)current_source_h,
				 -10000.0f, 10000.0f);
		} else {
			/* Fallback 2D para modo sin marcador. */
			gs_ortho(0.0f, (float)current_source_w, 0.0f, (float)current_source_h, -100.0f, 100.0f);
		}

		gs_matrix_push();
		gs_matrix_identity();

	
		gs_matrix_push();
		if (!overlay_use_3d_pose)
			gs_matrix_translate3f(final_x, final_y, 0.0f);

		if (align_overlay_to_aruco) {
			/* Mantener el overlay centrado en el marcador:
			 * - final_x/final_y es el centro del marcador en pantalla
			 * - aplicamos rotacion + escala segun marcador
			 * - trasladamos -tw/2,-th/2 para que el texto quede centrado (no su esquina)
			 */

			const float safe_tw = (tw > 0) ? (float)tw : 200.0f;
			const float safe_th = (th > 0) ? (float)th : 100.0f;

			float overlay_scale_x = 1.0f;
			float overlay_scale_y = 1.0f;
			/* Requisito funcional: tamano fijo decidido en UI, sin autoajuste al marcador. */
			overlay_scale_x = 1.0f;
			overlay_scale_y = 1.0f;

			/* Suavizado AR (solo para modos 3 y 4): reduce jitter en posicion/escala/angulo. */
			if (filter->overlay_ar_smooth_enabled) {
				const float a = clampf(filter->overlay_ar_smooth_alpha, 0.01f, 1.0f);

				if (!filter->overlay_ar_smooth_valid ||
				    filter->overlay_ar_smooth_marker_id != filter->last_result.id) {
					filter->overlay_ar_smooth_valid = true;
					filter->overlay_ar_smooth_marker_id = filter->last_result.id;
					filter->overlay_ar_smooth_x = final_x;
					filter->overlay_ar_smooth_y = final_y;
					filter->overlay_ar_smooth_scale =
						(overlay_scale_x + overlay_scale_y) * 0.5f;
					filter->overlay_ar_smooth_scale_x = overlay_scale_x;
					filter->overlay_ar_smooth_scale_y = overlay_scale_y;
					filter->overlay_ar_smooth_angle = angle_screen;
				} else {
					filter->overlay_ar_smooth_x =
						(1.0f - a) * filter->overlay_ar_smooth_x + a * final_x;
					filter->overlay_ar_smooth_y =
						(1.0f - a) * filter->overlay_ar_smooth_y + a * final_y;
					const float scale_avg =
						(overlay_scale_x + overlay_scale_y) * 0.5f;
					filter->overlay_ar_smooth_scale =
						(1.0f - a) * filter->overlay_ar_smooth_scale + a * scale_avg;
					filter->overlay_ar_smooth_scale_x =
						(1.0f - a) * filter->overlay_ar_smooth_scale_x + a * overlay_scale_x;
					filter->overlay_ar_smooth_scale_y =
						(1.0f - a) * filter->overlay_ar_smooth_scale_y + a * overlay_scale_y;

					const float d =
						wrap_angle_delta(filter->overlay_ar_smooth_angle, angle_screen);
					filter->overlay_ar_smooth_angle += a * d;
				}

				/* En modo 2D se suaviza traslacion en pantalla; en 3D no se mezcla con pixeles. */
				if (!overlay_use_3d_pose) {
					gs_matrix_translate3f(filter->overlay_ar_smooth_x - final_x,
							      filter->overlay_ar_smooth_y - final_y,
							      0.0f);
				}
				overlay_scale_x = filter->overlay_ar_smooth_scale_x;
				overlay_scale_y = filter->overlay_ar_smooth_scale_y;
				angle_screen = filter->overlay_ar_smooth_angle;
			}

			if (overlay_use_3d_pose) {
				/* 
				 *
				 * OBS post-multiplica matrices: el vértice se transforma de derecha a
				 * izquierda respecto al orden del código.  Colocando los offsets ANTES de
				 * R_x(180), R_aruco y R_x(90) en el código, se aplican AL ÚLTIMO sobre
				 * el vértice — es decir, después de que el panel ya está completamente
				 * orientado y centrado.  Resultado: los tres ejes del usuario son los ejes
				 * de PANTALLA, independientes entre sí y de la orientación del marcador:
				 *
				 *   ar_offset_rot_x → pitch  (eje horizontal de pantalla)
				 *   ar_offset_rot_y → yaw    (eje vertical de pantalla)
				 *   ar_offset_rot_z → roll   (eje que entra/sale por la pantalla)
				 *
				 * Como T_pos viene después en código (antes al vértice), las rotaciones
				 * se aplican sobre el panel centrado en el origen → giran respecto al
				 * centro del overlay, no respecto al origen de pantalla.
				 */
				const float overlay_pos_x = final_x + filter->ar_offset_pos_x;
				const float overlay_pos_y = final_y + filter->ar_offset_pos_y;
				const float overlay_pos_z = filter->ar_offset_pos_z;
				const float deg2rad = (float)M_PI / 180.0f;

				const float rvx = filter->last_result.rvec[0];
				const float rvy = filter->last_result.rvec[1];
				const float rvz = filter->last_result.rvec[2];
				float rangle = sqrtf(rvx * rvx + rvy * rvy + rvz * rvz);

				/* 1. Trasladar al centro del marcador. */
				gs_matrix_translate3f(overlay_pos_x, overlay_pos_y, overlay_pos_z);

				/* 2. Offsets manuales en frame de pantalla (ANTES de la cadena de
				 *    orientación: corrección OpenCV→OBS + rvec + levantado R_x(90)).
				 *    Por el orden de post-multiplicación, el vértice los recibe al final,
				 *    cuando el panel ya está en su pose definitiva → ejes de pantalla. */
				gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f,  filter->ar_offset_rot_x * deg2rad);
				gs_matrix_rotaa4f(0.0f, 1.0f, 0.0f, -filter->ar_offset_rot_y * deg2rad);  /* negar Y: OpenCV→OBS */
				gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, -filter->ar_offset_rot_z * deg2rad);  /* negar Z: OpenCV→OBS */

				/* 3. Corrección de ejes OpenCV → OBS. */
				gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, (float)M_PI);

				/* 4. Rotación de pose del marcador (rvec ArUco). */
				if (rangle > 1e-6f) {
					const float ax =  rvx / rangle;
					const float ay = -rvy / rangle;   /* negar Y: OpenCV→OBS */
					const float az = -rvz / rangle;   /* negar Z: OpenCV→OBS */
					gs_matrix_rotaa4f(ax, ay, az, rangle);
				}

				/* 5. Levantar el panel de texto perpendicular a la superficie del marcador. */
				gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, (float)M_PI / 2.0f);

			} else {
				/* Modo 2D fallback (marcador no detectado). */
				gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, angle_screen);
				gs_matrix_scale3f(overlay_scale_x, overlay_scale_y, 1.0f);
			}

			/* Anclaje clínico (Y-DOWN):
			 * final_x/final_y es el centro exacto de la cara del marcador.
			 * En modo AR: base del texto en el centro del marcador (texto sobre él).
			 * En modo 2D: texto centrado sobre el marcador.
			 */
			if (overlay_use_3d_pose) {
				/* El centro del marcador coincide con la base del texto. */
				gs_matrix_translate3f(-safe_tw * 0.5f, -safe_th, 0.0f);
			} else {
				/* Fallback 2D: centrado clásico. */
				gs_matrix_translate3f(-safe_tw * 0.5f, -safe_th * 0.5f, 0.0f);
			}

			/* Log de depuracion con throttling */
			static int log_throttle_overlay = 0;
			if ((log_throttle_overlay++ % 120) == 0) {
				blog(LOG_INFO,
				     "[CUBE-AR] Overlay marcador ID: %d (modo3d=%d centro_render=%.1f,%.1f centro_raw=%.1f,%.1f tvec=%.3f,%.3f,%.3f rvec=%.3f,%.3f,%.3f normal_tip=%.1f,%.1f normal_ok=%d marcador=%.1fx%.1f texto=%.0fx%.0f ang=%.2f rad)",
				     filter->last_result.id,
				     overlay_use_3d_pose ? 1 : 0,
				     final_x, final_y,
				     filter->last_result.screen_pos_x,
				     filter->last_result.screen_pos_y,
				     filter->last_result.tvec[0],
				     filter->last_result.tvec[1],
				     filter->last_result.tvec[2],
				     filter->last_result.rvec[0],
				     filter->last_result.rvec[1],
				     filter->last_result.rvec[2],
				     filter->last_result.normal_tip_x,
				     filter->last_result.normal_tip_y,
				     filter->last_result.normal_tip_valid ? 1 : 0,
				     have_marker_2d ? marker_w_px : -1.0f,
				     have_marker_2d ? marker_h_px : -1.0f,
				     safe_tw, safe_th, angle_screen);
			}
		}
		else {
			/* Si no hay marcador, invalidar suavizado para evitar arrastres. */
			filter->overlay_ar_smooth_valid = false;
			filter->overlay_ar_smooth_marker_id = -1;
		}

		/* Fondo estetico: se dibuja antes del texto y con el mismo transform (incluye AR si aplica). */
		if (filter->overlay_bg_enabled && filter->overlay_bg_vb) {
			const float safe_tw = (tw > 0) ? (float)tw : 200.0f;
			const float safe_th = (th > 0) ? (float)th : 100.0f;
			const float pad = (float)filter->overlay_bg_padding;
			const float bg_w = safe_tw + pad * 2.0f;
			const float bg_h = safe_th + pad * 2.0f;
			const int radius = filter->overlay_bg_radius;

			/* Color: UI devuelve 0xRRGGBB; opacity se aplica como alpha. */
			const uint32_t rgb = filter->overlay_bg_color;
			struct vec4 col_bg = {
				((float)((rgb >> 16) & 0xFF)) / 255.0f,
				((float)((rgb >> 8) & 0xFF)) / 255.0f,
				((float)(rgb & 0xFF)) / 255.0f,
				clampf(((float)filter->overlay_bg_opacity) / 100.0f, 0.0f, 1.0f),
			};

			gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
			gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
			gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");

			/* Preparar geometria redondeada si el radio > 0. */
			overlay_bg_update_rounded_geometry(filter, bg_w, bg_h, radius);
			const bool use_round = (radius > 0 && filter->overlay_bg_round_vb);
			gs_vertbuffer_t *vb = use_round ? filter->overlay_bg_round_vb : filter->overlay_bg_vb;
			const uint32_t draw_count = use_round ? 0 : 6;

			/* Sombra "suave" por capas (sin shader de blur). */
			if (filter->overlay_bg_shadow_enabled) {
				const int layers = clampi(filter->overlay_bg_shadow_softness, 1, 8);
				const float base_a = clampf(((float)filter->overlay_bg_shadow_opacity) / 100.0f, 0.0f, 1.0f);
				const float offx = (float)filter->overlay_bg_shadow_offset_x;
				const float offy = (float)filter->overlay_bg_shadow_offset_y;
				const float denom = (layers > 1) ? (float)(layers - 1) : 1.0f;

				for (int i = 0; i < layers; i++) {
					const float t = (layers > 1) ? ((float)i / denom) : 0.0f; /* 0..1 */
					const float a = base_a * (1.0f - t) * (1.0f - t);
					if (!(a > 0.001f))
						continue;

					struct vec4 col_shadow = {0.0f, 0.0f, 0.0f, a};
					gs_matrix_push();
					gs_matrix_translate3f(-pad + offx * (0.5f + t),
							      -pad + offy * (0.5f + t),
							      0.0f);
					if (!use_round)
						gs_matrix_scale3f(bg_w, bg_h, 1.0f);

					gs_technique_begin(tech);
					gs_technique_begin_pass(tech, 0);
					gs_effect_set_vec4(color_param, &col_shadow);
					gs_load_vertexbuffer(vb);
					gs_draw(GS_TRIS, 0, draw_count);
					gs_technique_end_pass(tech);
					gs_technique_end(tech);
					gs_matrix_pop();
				}
			}

			/* Fondo principal */
			gs_matrix_push();
			gs_matrix_translate3f(-pad, -pad, 0.0f);
			if (!use_round)
				gs_matrix_scale3f(bg_w, bg_h, 1.0f);

			gs_technique_begin(tech);
			gs_technique_begin_pass(tech, 0);
			gs_effect_set_vec4(color_param, &col_bg);
			gs_load_vertexbuffer(vb);
			gs_draw(GS_TRIS, 0, draw_count);
			gs_technique_end_pass(tech);
			gs_technique_end(tech);

			gs_matrix_pop();

			/* Modo Scoreboard (3): dibujar tabla con cabecera y bandas por fila.
			 * Esto evita que se vea como "texto pegado" y no depende de caracteres tipo '---'. */
			if (filter->mode == 3 && filter->scoreboard_row_stripes &&
			    filter->scoreboard_line_count > 0 &&
			    filter->scoreboard_row_count > 0) {
				const int lines = filter->scoreboard_line_count;
				const int header_lines =
					(filter->scoreboard_header_lines > 0)
						? filter->scoreboard_header_lines
						: 1;
				const float line_h = (safe_th > 1.0f)
							     ? (safe_th / (float)lines)
							     : 1.0f;
				const float header_h = line_h * (float)header_lines;

				float cr = 1.0f, cg = 1.0f, cb = 1.0f;
				overlay_pick_contrast_rgb(col_bg.x, col_bg.y, col_bg.z,
							  &cr, &cg, &cb);

				float stripe_a = clampf(((float)filter->scoreboard_row_stripe_opacity) / 100.0f,
							0.0f, 0.8f);
				/* Ajuste para que el valor de UI sea util sin tapar el texto. */
				stripe_a = clampf(stripe_a * 0.65f, 0.0f, 0.65f);

				struct vec4 col_header = {cr, cg, cb, clampf(stripe_a * 1.6f, 0.0f, 0.75f)};
				struct vec4 col_stripe = {cr, cg, cb, stripe_a};

				/* Barra de cabecera: cubre padding superior + 1a linea. */
				gs_matrix_push();
				gs_matrix_translate3f(-pad, -pad, 0.0f);
				gs_matrix_scale3f(bg_w, header_h + pad, 1.0f);
				gs_technique_begin(tech);
				gs_technique_begin_pass(tech, 0);
				gs_effect_set_vec4(color_param, &col_header);
				gs_load_vertexbuffer(filter->overlay_bg_vb);
				gs_draw(GS_TRIS, 0, 6);
				gs_technique_end_pass(tech);
				gs_technique_end(tech);
				gs_matrix_pop();

				/* Bandas alternas: solo filas de datos (no cabecera). */
				for (int r = 0; r < filter->scoreboard_row_count; r++) {
					if ((r % 2) == 0)
						continue;

					const float y = header_h + (float)r * line_h;
					gs_matrix_push();
					gs_matrix_translate3f(-pad, y, 0.0f);
					gs_matrix_scale3f(bg_w, line_h, 1.0f);
					gs_technique_begin(tech);
					gs_technique_begin_pass(tech, 0);
					gs_effect_set_vec4(color_param, &col_stripe);
					gs_load_vertexbuffer(filter->overlay_bg_vb);
					gs_draw(GS_TRIS, 0, 6);
					gs_technique_end_pass(tech);
					gs_technique_end(tech);
					gs_matrix_pop();
				}
			}
		}

		if (filter->mode == 3) {
			float x_cursor = 0.0f;

			gs_matrix_push();
			gs_matrix_translate3f(x_cursor, 0.0f, 0.0f);
			obs_source_video_render(filter->sb_pos_source);
			gs_matrix_pop();
			x_cursor += (float)w_pos + col_gap;

			gs_matrix_push();
			gs_matrix_translate3f(x_cursor, 0.0f, 0.0f);
			obs_source_video_render(filter->sb_name_source);
			gs_matrix_pop();
			x_cursor += (float)w_name + col_gap;

			gs_matrix_push();
			gs_matrix_translate3f(x_cursor, 0.0f, 0.0f);
			obs_source_video_render(filter->sb_solved_source);
			gs_matrix_pop();
			x_cursor += (float)w_res + col_gap;

			gs_matrix_push();
			gs_matrix_translate3f(x_cursor, 0.0f, 0.0f);
			obs_source_video_render(filter->sb_time_source);
			gs_matrix_pop();
		} else {
			obs_source_video_render(filter->scoreboard_text_source);
		}
		gs_matrix_pop();

		gs_matrix_pop();
		gs_projection_pop();
		gs_set_cull_mode(prev_cull_mode);
		gs_blend_state_pop();
		gs_enable_depth_test(true);

		if (filter->mode == 4) {
			 int log_throttle_render = 0;
			if (log_throttle_render++ % 60 == 0) {
				blog(LOG_INFO, "[CUBE-TEAM-INFO-RENDER] Dibujando en X: %.1f, Y: %.1f", final_x, final_y);
			}
		}

		if (tw == 0 || th == 0) {
			 int log_throttle = 0;
			if (log_throttle++ % 120 == 0) {
				blog(LOG_DEBUG, "[CUBE] Text source size is 0x0 (not ready)");
			}
		}
	}

	obs_leave_graphics();
}

/* Updates property visibility according to the selected render mode. */
/* Actualiza la visibilidad de las propiedades segun el modo de render seleccionado. */
static bool render_mode_changed(obs_properties_t *props,
				obs_property_t *property, obs_data_t *settings)
{
	int mode = (int)obs_data_get_int(settings, "render_mode");
	bool show_3d = (mode == 0);
	bool show_ar = (mode == 1);
	bool show_countdown = (mode == 2);
	bool show_scoreboard = (mode == 3);
	bool show_team_info = (mode == 4);

	bool countdown_use_ar = obs_data_get_bool(settings, "countdown_use_ar");
	int clock_mode = (int)obs_data_get_int(settings, "clock_mode");
	

	bool show_manual_pos = show_3d || (show_countdown && !countdown_use_ar);
	obs_property_set_visible(obs_properties_get(props, "pos_x"), show_manual_pos);
	obs_property_set_visible(obs_properties_get(props, "pos_y"), show_manual_pos);
	obs_property_set_visible(obs_properties_get(props, "pos_z"), show_manual_pos);
	obs_property_set_visible(obs_properties_get(props, "rotation_x_slider_value"), show_manual_pos);
	obs_property_set_visible(obs_properties_get(props, "rotation_y_slider_value"), show_manual_pos);
	obs_property_set_visible(obs_properties_get(props, "rotation_z_slider_value"), show_manual_pos);

	
	bool show_ar_controls = show_ar || (show_countdown && countdown_use_ar);
	obs_property_set_visible(obs_properties_get(props, "marker_id"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "marker_size"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "marker_dict"), show_ar_controls || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "calibration_path"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_pos_x"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_pos_y"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_pos_z"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_rot_x"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_rot_y"), show_ar_controls);
	obs_property_set_visible(obs_properties_get(props, "ar_offset_rot_z"), show_ar_controls);

	bool show_3d_common = show_3d || show_ar || show_countdown;
	obs_property_set_visible(obs_properties_get(props, "scale"), show_3d_common);
	obs_property_set_visible(obs_properties_get(props, "texture_path"), show_3d_common);
	obs_property_set_visible(obs_properties_get(props, "model_path"), show_3d_common);

	/* Countdown: duracion, ejecucion, sincronizacion web */
	obs_property_set_visible(obs_properties_get(props, "countdown_duration_h"), show_countdown);
	obs_property_set_visible(obs_properties_get(props, "countdown_duration_m"), show_countdown);
	obs_property_set_visible(obs_properties_get(props, "countdown_duration_s"), show_countdown);
	obs_property_set_visible(obs_properties_get(props, "countdown_running"), show_countdown);
	obs_property_set_visible(obs_properties_get(props, "countdown_reset"), show_countdown);
	
	/* Shared Sync settings for Countdown or Scoreboard */
	bool show_sync = show_countdown || show_scoreboard || show_team_info;
	obs_property_set_visible(obs_properties_get(props, "sync_enabled"), show_sync);
	obs_property_set_visible(obs_properties_get(props, "sync_interval_sec"), show_sync);

/* DOMjudge API. */
	/* DOMjudge API */
	obs_property_set_visible(obs_properties_get(props, "api_base_url"), show_sync);
	obs_property_set_visible(obs_properties_get(props, "contest_id"), show_sync);
	obs_property_set_visible(obs_properties_get(props, "api_username"), show_sync);
	obs_property_set_visible(obs_properties_get(props, "api_password"), show_sync);
	obs_property_set_visible(obs_properties_get(props, "test_connection"), show_sync);

	/* Scoreboard Specific */
	obs_property_set_visible(obs_properties_get(props, "scoreboard_offset_x"), show_scoreboard);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_offset_y"), show_scoreboard);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_centered"), show_scoreboard);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_font_size"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_font_face"), false); // Eliminado a peticion del usuario
	obs_property_set_visible(obs_properties_get(props, "scoreboard_text_color"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_outline_color"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_outline_size"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_name_max_chars"), show_scoreboard);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_row_stripes"), show_scoreboard);
	obs_property_set_visible(obs_properties_get(props, "scoreboard_row_stripe_opacity"), show_scoreboard);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_enabled"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_color"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_opacity"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_padding"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_radius"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_shadow_enabled"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_shadow_opacity"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_shadow_offset_x"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_shadow_offset_y"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_bg_shadow_softness"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_ar_smooth_enabled"), show_scoreboard || show_team_info);
	obs_property_set_visible(obs_properties_get(props, "overlay_ar_smooth_alpha"), show_scoreboard || show_team_info);

	/* Team Info mode: selector JSON local + controles AR */
	obs_property_set_visible(obs_properties_get(props, "team_info_json_path"), show_team_info);
	obs_property_set_visible(obs_properties_get(props, "team_info_json_reload"), show_team_info);
	/* En Team Info, reutilizar controles AR para configurar la deteccion */
	if (show_team_info) {
		obs_property_set_visible(obs_properties_get(props, "marker_dict"), true);
	}

	
	obs_property_set_visible(obs_properties_get(props, "clock_mode"), show_countdown);
	obs_property_set_visible(obs_properties_get(props, "countdown_use_ar"), show_countdown);
	//obs_property_set_visible(obs_properties_get(props, "mesh_id_dial"), show_countdown);
	
	// Mostrar IDs de manecillas segÃºn el modo de reloj
	bool show_three_hands = show_countdown && (clock_mode == 0);
	bool show_single_hand = show_countdown && (clock_mode == 1);
	obs_property_set_visible(obs_properties_get(props, "mesh_id_hour_hand"), show_three_hands);
	obs_property_set_visible(obs_properties_get(props, "mesh_id_minute_hand"), show_three_hands);
	obs_property_set_visible(obs_properties_get(props, "mesh_id_second_hand"), show_three_hands);
	obs_property_set_visible(obs_properties_get(props, "mesh_id_single_hand"), show_single_hand);

	return true;
}

/* Callback para el botón "Probar Conexion" de DOMjudge */
/* Tests the DOMjudge connection using the current settings. */
/* Prueba la conexion con DOMjudge usando la configuracion actual. */
static bool test_connection_callback(obs_properties_t *props,
				     obs_property_t *property, void *data)
{
	struct cube_filter_data *filter = data;
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);

	const char *base = filter->api_base_url;
	const char *cid = filter->contest_id;

	if (!base || !base[0] || !cid || !cid[0]) {
		blog(LOG_WARNING,
		     "[CUBE] Probar Conexion: URL base o ID de torneo vacios");
		return false;
	}

	blog(LOG_INFO, "[CUBE] Probando conexion a %s/contests/%s", base, cid);
	bool ok = web_sync_test_connection(base, cid, filter->api_username, filter->api_password);
	if (ok) {
		blog(LOG_INFO, "[CUBE] Resultado Prueba: EXITO. El servidor respondio correctamente.");
	} else {
		blog(LOG_WARNING, "[CUBE] Resultado Prueba: FALLO. Consulte los logs de [WEB_SYNC] mas arriba para detalles.");
	}
	return false; /* no refrescar la UI */
}

/* Reloads Team Info mappings from the configured JSON file. */
/* Recarga los mapeos de Team Info desde el archivo JSON configurado. */
static bool team_info_reload_json_callback(obs_properties_t *props,
					  obs_property_t *property, void *data)
{
	struct cube_filter_data *filter = data;
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);

	if (!filter) {
		blog(LOG_WARNING, "[CUBE-TEAM-INFO] Recarga JSON: filtro NULL");
		return false;
	}

	/* Leer la ruta actual desde los settings del source para reflejar la UI */
	obs_data_t *cur_settings = obs_source_get_settings(filter->source);
	const char *path = "";
	if (cur_settings) {
		path = obs_data_get_string(cur_settings, "team_info_json_path");
		if (!path)
			path = "";
	}
	blog(LOG_INFO, "[CUBE-TEAM-INFO] Recarga manual solicitada. Ruta: %s",
	     path[0] ? path : "(vacia)");

	/* Mantener la ruta en la estructura del filtro consistente con los settings */
	bfree(filter->team_info_json_path);
	filter->team_info_json_path = (path[0]) ? bstrdup(path) : NULL;

	if (!team_info_load_json_from_path(filter, path)) {
		blog(LOG_WARNING,
		     "[CUBE-TEAM-INFO] Recarga manual fallida (se mantiene el mapeo anterior si existia)");
	}

	if (cur_settings)
		obs_data_release(cur_settings);
	return false; /* no refrescar la UI */
}

/* Builds the property list shown in the OBS filter settings panel. */
/* Construye la lista de propiedades mostrada en el panel de configuracion del filtro de OBS. */
static obs_properties_t *filter_properties(void *data)
{

	obs_properties_t *props = obs_properties_create();
	obs_property_t *combo = obs_properties_add_list(props, "render_mode",
							"Modo de renderizado",
							OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(combo, "3D", 0);
	obs_property_list_add_int(combo, "AR", 1);
	obs_property_list_add_int(combo, "Countdown (reloj)", 2);
	obs_property_list_add_int(combo, "Scoreboard (texto)", 3);
	obs_property_list_add_int(combo, "Team Info", 4);
	obs_property_set_modified_callback(combo, render_mode_changed);

	// Propiedades 3D
	obs_properties_add_float_slider(props, "pos_x", "Posicion X", -3000.0f,3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "pos_y", "Posicion Y", -3000.0f,3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "pos_z", "Posicion Z", -3000.0f,3000.0f, 10.0f);
	obs_properties_add_float_slider(props, "scale", "Escala", 0.1f, 1000.0f,0.01f);
	obs_properties_add_float_slider(props, "rotation_x_slider_value","Rotacion X (Grados)", -360.0f, 360.0f,1.0f);
	obs_properties_add_float_slider(props, "rotation_y_slider_value","Rotacion Y (Grados)", -360.0f, 360.0f,1.0f);
	obs_properties_add_float_slider(props, "rotation_z_slider_value","Rotacion Z (Grados)", -360.0f, 360.0f,1.0f);

	// Propiedades Comunes
	obs_properties_add_path(props, "texture_path", "Ruta de la Textura", OBS_PATH_FILE,"Imagenes (*.png *.jpg *.jpeg *.bmp *.tga);;Todos (*.*)", NULL);
	obs_properties_add_path(props, "model_path", "Ruta del Modelo 3D", OBS_PATH_FILE,"Modelos 3D (*.obj *.fbx *.dae *.gltf);;Todos (*.*)", NULL);

	// Propiedades AR
	obs_properties_add_int(props, "marker_id", "ID del Marker", 0, 100, 1);
	obs_properties_add_float_slider(props, "marker_size",	"Tamano del Marker", 0.1f, 10.0f, 0.1f);
	obs_property_t *dict = obs_properties_add_list(props, "marker_dict", "Diccionario de Marker", OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(dict, "Original", ARUCO_DICT_ORIGINAL);
	obs_property_list_add_int(dict, "4x4 (100)", ARUCO_DICT_4X4_100);
	obs_property_list_add_int(dict, "5x5 (100)", ARUCO_DICT_5X5_100);
	obs_property_list_add_int(dict, "6x6 (100)", ARUCO_DICT_6X6_100);
	obs_property_list_add_int(dict, "7x7 (100)", ARUCO_DICT_7X7_100);
obs_properties_add_path(props, "calibration_path", "Archivo de Calibracion", 
                        OBS_PATH_FILE, "YAML (*.yml *.yaml);;Todos (*.*)", NULL);

	obs_properties_add_float_slider(props, "ar_offset_pos_x","AR Offset Posicion X", -1000,1000.0f, 10.0f);
	obs_properties_add_float_slider(props, "ar_offset_pos_y","AR Offset Posicion Y", -1000.0f,1000.0f, 10.0f);
	obs_properties_add_float_slider(props, "ar_offset_pos_z","AR Offset Posicion Z", -1000.0f,1000.0f, 10.0f);
	obs_properties_add_float_slider(props, "ar_offset_rot_x","AR Offset Rotacion X", -360.0f, 360.0f,1.0f);
	obs_properties_add_float_slider(props, "ar_offset_rot_y","AR Offset Rotacion Y", -360.0f, 360.0f,1.0f);
	obs_properties_add_float_slider(props, "ar_offset_rot_z","AR Offset Rotacion Z", -360.0f, 360.0f,1.0f);

	/* Countdown: duracion y sincronizacion web */
	obs_properties_add_int(props, "countdown_duration_h", "Horas", 0, 99, 1);
	obs_properties_add_int(props, "countdown_duration_m", "Minutos", 0, 59, 1);
	obs_properties_add_int(props, "countdown_duration_s", "Segundos", 0, 59, 1);
	obs_properties_add_bool(props, "countdown_running", "Cuenta Atras a Mover");
	obs_properties_add_bool(props, "countdown_reset", "Reiniciar Reloj");
	obs_properties_add_bool(props, "sync_enabled", "Sincronizacion Web");
	obs_properties_add_float(props, "sync_interval_sec", "Intervalo API (seg)", 1.0f, 300.0f, 1.0f);

	/* --- CONFIGURACION RELOJ --- */
	obs_property_t *clk_mode = obs_properties_add_list(props, "clock_mode", "Manecillas Reloj", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(clk_mode, "H/M/S (3)", 0);
	obs_property_list_add_int(clk_mode, "Total (1)", 1);
	
	obs_properties_add_int(props, "countdown_duration_h", "CD Horas", 0, 99, 1);
	obs_properties_add_int(props, "countdown_duration_m", "CD Minutos", 0, 59, 1);
	obs_properties_add_int(props, "countdown_duration_s", "CD Segundos", 0, 59, 1);
	obs_properties_add_bool(props, "countdown_running", "Iniciar Cuenta Atras");
	obs_properties_add_bool(props, "countdown_reset", "Reiniciar Reloj");
	obs_properties_add_bool(props, "countdown_use_ar", "Reloj en Marker AR");

	/* --- DOMJUDGE & SYNC --- */
	obs_properties_add_bool(props, "sync_enabled", "Sincronizar con DOMjudge");
	obs_properties_add_text(props, "api_base_url", "URL API (DOMjudge)", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "contest_id", "Contest ID", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "api_username", "Usuario API", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "api_password", "Contrasena API", OBS_TEXT_PASSWORD);
	obs_properties_add_button(props, "test_connection", "Probar Conexion", test_connection_callback);
	obs_properties_add_float(props, "sync_interval_sec", "Intervalo Sinc (seg)", 1.0f, 300.0f, 1.0f);

	/* --- SCOREBOARD OVERLAY --- */
	obs_properties_add_bool(props, "scoreboard_centered", "Centrar en Pantalla");
	obs_properties_add_float_slider(props, "scoreboard_offset_x", "Offset Manual X", 0.0f, 3000.0f, 5.0f);
	obs_properties_add_float_slider(props, "scoreboard_offset_y", "Offset Manual Y", 0.0f, 3000.0f, 5.0f);
	obs_properties_add_int(props, "scoreboard_font_size", "Tamano de Fuente", 5, 150, 1);
	obs_properties_add_text(props, "scoreboard_font_face", "Fuente (Arial, Consolas...)", OBS_TEXT_DEFAULT);
	obs_properties_add_color(props, "scoreboard_text_color", "Color del Texto");
	obs_properties_add_color(props, "scoreboard_outline_color", "Color del Borde");
	obs_properties_add_int(props, "scoreboard_outline_size", "Grosor del Borde", 0, 20, 1);
	obs_properties_add_int_slider(props, "scoreboard_name_max_chars", "Max caracteres equipo", 5, 40, 1);
	obs_properties_add_bool(props, "scoreboard_row_stripes", "Bandas por fila");
	obs_properties_add_int_slider(props, "scoreboard_row_stripe_opacity", "Opacidad bandas (%)", 0, 80, 1);

	/* Fondo del overlay de texto (Scoreboard/Team Info) */
	obs_properties_add_bool(props, "overlay_bg_enabled", "Activar fondo del texto");
	obs_properties_add_color(props, "overlay_bg_color", "Color de fondo");
	obs_properties_add_int_slider(props, "overlay_bg_opacity", "Opacidad fondo (%)", 0, 100, 1);
	obs_properties_add_int_slider(props, "overlay_bg_padding", "Margen interno (px)", 0, 80, 1);
	obs_properties_add_int_slider(props, "overlay_bg_radius", "Radio esquinas (px)", 0, 80, 1);

	obs_properties_add_bool(props, "overlay_bg_shadow_enabled", "Activar sombra");
	obs_properties_add_int_slider(props, "overlay_bg_shadow_opacity", "Opacidad sombra (%)", 0, 100, 1);
	obs_properties_add_int_slider(props, "overlay_bg_shadow_offset_x", "Sombra offset X (px)", -50, 50, 1);
	obs_properties_add_int_slider(props, "overlay_bg_shadow_offset_y", "Sombra offset Y (px)", -50, 50, 1);
	obs_properties_add_int_slider(props, "overlay_bg_shadow_softness", "Sombra suavidad (capas)", 1, 8, 1);

	obs_properties_add_bool(props, "overlay_ar_smooth_enabled", "Suavizado AR (overlay)");
	obs_properties_add_float_slider(props, "overlay_ar_smooth_alpha", "Suavizado AR (alpha)", 0.01f, 1.0f, 0.01f);

	/* --- TEAM INFO (JSON LOCAL) --- */
	obs_properties_add_path(props, "team_info_json_path",
				"JSON de equipos (ArUco->Team)",
				OBS_PATH_FILE,
				"JSON (*.json);;Todos (*.*)",
				NULL);
	obs_properties_add_button(props, "team_info_json_reload",
				  "Recargar JSON", team_info_reload_json_callback);

	obs_data_t *temp_settings = obs_data_create();
	render_mode_changed(props, combo, temp_settings);
	obs_data_release(temp_settings);
	return props;
}

/* Applies updated settings to the live filter state. */
/* Aplica la configuracion actualizada al estado activo del filtro. */
static void filter_update(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	if (!filter || !settings)return;

	
	filter->mode = (int)obs_data_get_int(settings, "render_mode");
	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->countdown_running = obs_data_get_bool(settings, "countdown_running");
	filter->countdown_reset_requested = obs_data_get_bool(settings, "countdown_reset");
	filter->countdown_duration_h = (uint32_t)obs_data_get_int(settings, "countdown_duration_h");
	filter->countdown_duration_m = (uint32_t)obs_data_get_int(settings, "countdown_duration_m");
	filter->countdown_duration_s = (uint32_t)obs_data_get_int(settings, "countdown_duration_s");
	filter->sync_enabled = obs_data_get_bool(settings, "sync_enabled");
	filter->sync_interval_sec = (float)obs_data_get_double(settings, "sync_interval_sec");
	
	filter->clock_mode = (int)obs_data_get_int(settings, "clock_mode");
	filter->mesh_id_dial = (int)obs_data_get_int(settings, "mesh_id_dial");
	filter->mesh_id_hour_hand = (int)obs_data_get_int(settings, "mesh_id_hour_hand");
	filter->mesh_id_minute_hand = (int)obs_data_get_int(settings, "mesh_id_minute_hand");
	filter->mesh_id_second_hand = (int)obs_data_get_int(settings, "mesh_id_second_hand");
	filter->mesh_id_single_hand = (int)obs_data_get_int(settings, "mesh_id_single_hand");
	filter->countdown_use_ar = obs_data_get_bool(settings, "countdown_use_ar");
	
	if (filter->countdown_clock) {
		blog(LOG_INFO, "SET DURATION");
		countdown_clock_set_duration_hms(filter->countdown_clock,filter->countdown_duration_h, filter->countdown_duration_m, filter->countdown_duration_s);
		countdown_state_t state = countdown_clock_get_state(filter->countdown_clock);
		
		if (filter->countdown_running) {
			if (state == COUNTDOWN_STATE_STOPPED)
				countdown_clock_start(filter->countdown_clock);
			else if (state == COUNTDOWN_STATE_PAUSED)
				countdown_clock_resume(filter->countdown_clock);
		} else {
			if (state == COUNTDOWN_STATE_RUNNING)
				countdown_clock_pause(filter->countdown_clock);
		}
	}
	/* DOMjudge API: leer campos */
	const char *new_base_url = obs_data_get_string(settings, "api_base_url");
	if (!new_base_url) new_base_url = "";
	const char *new_contest_id = obs_data_get_string(settings, "contest_id");
	if (!new_contest_id) new_contest_id = "";

	bfree(filter->api_base_url);
	filter->api_base_url = (new_base_url[0]) ? bstrdup(new_base_url) : NULL;
	bfree(filter->contest_id);
	filter->contest_id = (new_contest_id[0]) ? bstrdup(new_contest_id) : NULL;

	const char *new_user = obs_data_get_string(settings, "api_username");
	const char *new_pass = obs_data_get_string(settings, "api_password");
	bfree(filter->api_username);
	filter->api_username = (new_user && new_user[0]) ? bstrdup(new_user) : NULL;
	bfree(filter->api_password);
	filter->api_password = (new_pass && new_pass[0]) ? bstrdup(new_pass) : NULL;

	filter->scoreboard_offset_x = (float)obs_data_get_double(settings, "scoreboard_offset_x");
	filter->scoreboard_offset_y = (float)obs_data_get_double(settings, "scoreboard_offset_y");
	filter->scoreboard_centered = obs_data_get_bool(settings, "scoreboard_centered");
	filter->scoreboard_font_size = (int)obs_data_get_int(settings, "scoreboard_font_size");
	bfree(filter->scoreboard_font_face);
	const char *ff_update = obs_data_get_string(settings, "scoreboard_font_face");
	filter->scoreboard_font_face = (ff_update && ff_update[0]) ? bstrdup(ff_update) : NULL;
	filter->scoreboard_text_color = (uint32_t)obs_data_get_int(settings, "scoreboard_text_color");
	filter->scoreboard_outline_color = (uint32_t)obs_data_get_int(settings, "scoreboard_outline_color");
	filter->scoreboard_outline_size = (int)obs_data_get_int(settings, "scoreboard_outline_size");

	/* Scoreboard tabla (modo 3): opciones de presentacion */
	filter->scoreboard_name_max_chars =
		(int)obs_data_get_int(settings, "scoreboard_name_max_chars");
	filter->scoreboard_name_max_chars =
		clampi(filter->scoreboard_name_max_chars, 5, 40);
	filter->scoreboard_row_stripes =
		obs_data_get_bool(settings, "scoreboard_row_stripes");
	filter->scoreboard_row_stripe_opacity =
		(int)obs_data_get_int(settings, "scoreboard_row_stripe_opacity");
	filter->scoreboard_row_stripe_opacity =
		clampi(filter->scoreboard_row_stripe_opacity, 0, 80);
	filter->overlay_bg_enabled = obs_data_get_bool(settings, "overlay_bg_enabled");
	filter->overlay_bg_color = (uint32_t)obs_data_get_int(settings, "overlay_bg_color");
	filter->overlay_bg_opacity = (int)obs_data_get_int(settings, "overlay_bg_opacity");
	filter->overlay_bg_padding = (int)obs_data_get_int(settings, "overlay_bg_padding");
	filter->overlay_bg_radius = (int)obs_data_get_int(settings, "overlay_bg_radius");
	filter->overlay_bg_shadow_enabled = obs_data_get_bool(settings, "overlay_bg_shadow_enabled");
	filter->overlay_bg_shadow_opacity = (int)obs_data_get_int(settings, "overlay_bg_shadow_opacity");
	filter->overlay_bg_shadow_offset_x = (int)obs_data_get_int(settings, "overlay_bg_shadow_offset_x");
	filter->overlay_bg_shadow_offset_y = (int)obs_data_get_int(settings, "overlay_bg_shadow_offset_y");
	filter->overlay_bg_shadow_softness = (int)obs_data_get_int(settings, "overlay_bg_shadow_softness");
	filter->overlay_ar_smooth_enabled = obs_data_get_bool(settings, "overlay_ar_smooth_enabled");
	filter->overlay_ar_smooth_alpha = (float)obs_data_get_double(settings, "overlay_ar_smooth_alpha");

	/* Si se desactiva el suavizado, invalidar el estado para evitar "saltos" al reactivar. */
	if (!filter->overlay_ar_smooth_enabled) {
		filter->overlay_ar_smooth_valid = false;
		filter->overlay_ar_smooth_marker_id = -1;
	}

	/* Team Info (JSON local): ruta y carga (evita trabajo en hilo de render) */
	const char *new_json_path =
		obs_data_get_string(settings, "team_info_json_path");
	if (!new_json_path)
		new_json_path = "";

	bool json_path_changed = false;
	if (!filter->team_info_json_path && new_json_path[0]) {
		json_path_changed = true;
	} else if (filter->team_info_json_path &&
		   strcmp(filter->team_info_json_path, new_json_path) != 0) {
		json_path_changed = true;
	}

	if (json_path_changed) {
		blog(LOG_INFO, "[CUBE-TEAM-INFO] Nueva ruta JSON local: %s",
	     new_json_path[0] ? new_json_path : "(vacia)");
	}

	bfree(filter->team_info_json_path);
	filter->team_info_json_path =
		(new_json_path[0]) ? bstrdup(new_json_path) : NULL;

	/* Cargar solo si cambia la ruta (la recarga manual se hace con el boton) */
	if (json_path_changed) {
		if (!team_info_load_json_from_path(
			    filter, filter->team_info_json_path
					    ? filter->team_info_json_path
					    : "")) {
			blog(LOG_WARNING,
			     "[CUBE-TEAM-INFO] No se pudo cargar el JSON (se mantiene el mapeo anterior si existia)");
		}
	}

	/* Crear fuente de texto para modo Scoreboard (3) o Team Info (4) */
	if ((filter->mode == 3 || filter->mode == 4) && !filter->scoreboard_text_source) {
		obs_data_t *txt_settings = obs_data_create();
		if (filter->mode == 3)
			obs_data_set_string(txt_settings, "text",
					    "[[ MODO SCOREBOARD ACTIVO ]]\nEsperando datos...");
		else
			obs_data_set_string(txt_settings, "text",
					    "[[ MODO TEAM INFO ACTIVO ]]\nEsperando Deteccion de ArUco...");
		obs_data_set_int(txt_settings, "color", filter->scoreboard_text_color);
		obs_data_set_int(txt_settings, "font_size", filter->scoreboard_font_size);
		obs_data_set_string(txt_settings, "font_face", filter->scoreboard_font_face ? filter->scoreboard_font_face : "Arial");
		obs_data_set_int(txt_settings, "opacity", 100);
		obs_data_set_bool(txt_settings, "outline", true);
		obs_data_set_int(txt_settings, "outline_size", filter->scoreboard_outline_size);
		obs_data_set_int(txt_settings, "outline_color", filter->scoreboard_outline_color);
		
		filter->scoreboard_text_source = obs_source_create_private("text_gdiplus_v2", "scoreboard_text", txt_settings);
		if (!filter->scoreboard_text_source) {
			filter->scoreboard_text_source = obs_source_create_private("text_gdiplus", "scoreboard_text", txt_settings);
		}

		if (filter->scoreboard_text_source) {
			blog(LOG_INFO, "[CUBE] Fuente de texto overlay (reloj/scoreboard/teaminfo) creada");
			obs_source_add_active_child(filter->source, filter->scoreboard_text_source);
		}
		obs_data_release(txt_settings);
	}

	/* Scoreboard modo 3: columnas separadas en sources distintos (POS/EQUIPO/RES/TIEMPO). */
	if (filter->mode == 3 &&
	    (!filter->sb_pos_source || !filter->sb_name_source ||
	     !filter->sb_solved_source || !filter->sb_time_source)) {
		char name_pos[64], name_name[64], name_res[64], name_time[64];
		snprintf(name_pos, sizeof(name_pos), "sb_pos_%p", (void *)filter);
		snprintf(name_name, sizeof(name_name), "sb_name_%p", (void *)filter);
		snprintf(name_res, sizeof(name_res), "sb_res_%p", (void *)filter);
		snprintf(name_time, sizeof(name_time), "sb_time_%p", (void *)filter);

		obs_data_t *s_pos = obs_data_create();
		obs_data_t *s_name = obs_data_create();
		obs_data_t *s_res = obs_data_create();
		obs_data_t *s_time = obs_data_create();
		obs_data_t *font_obj = obs_data_create();

		const int fs = (filter->scoreboard_font_size > 0) ? filter->scoreboard_font_size : 25;

		obs_data_set_int(font_obj, "size", fs);
		obs_data_set_string(font_obj, "face",
				    (filter->scoreboard_font_face && filter->scoreboard_font_face[0])
					    ? filter->scoreboard_font_face
					    : "Arial");

		obs_data_set_obj(s_pos, "font", font_obj);
		obs_data_set_obj(s_name, "font", font_obj);
		obs_data_set_obj(s_res, "font", font_obj);
		obs_data_set_obj(s_time, "font", font_obj);

		obs_data_set_int(s_pos, "color", filter->scoreboard_text_color);
		obs_data_set_int(s_name, "color", filter->scoreboard_text_color);
		obs_data_set_int(s_res, "color", filter->scoreboard_text_color);
		obs_data_set_int(s_time, "color", filter->scoreboard_text_color);

		obs_data_set_bool(s_pos, "outline", true);
		obs_data_set_bool(s_name, "outline", true);
		obs_data_set_bool(s_res, "outline", true);
		obs_data_set_bool(s_time, "outline", true);
		obs_data_set_int(s_pos, "outline_size", filter->scoreboard_outline_size);
		obs_data_set_int(s_name, "outline_size", filter->scoreboard_outline_size);
		obs_data_set_int(s_res, "outline_size", filter->scoreboard_outline_size);
		obs_data_set_int(s_time, "outline_size", filter->scoreboard_outline_size);
		obs_data_set_int(s_pos, "outline_color", filter->scoreboard_outline_color);
		obs_data_set_int(s_name, "outline_color", filter->scoreboard_outline_color);
		obs_data_set_int(s_res, "outline_color", filter->scoreboard_outline_color);
		obs_data_set_int(s_time, "outline_color", filter->scoreboard_outline_color);

		/* Alineación por columna. */
		obs_data_set_int(s_pos, "align", 2);
		obs_data_set_int(s_name, "align", 0);
		obs_data_set_int(s_res, "align", 2);
		obs_data_set_int(s_time, "align", 2);
		obs_data_set_bool(s_pos, "extents", false);
		obs_data_set_bool(s_name, "extents", false);
		obs_data_set_bool(s_res, "extents", false);
		obs_data_set_bool(s_time, "extents", false);

		obs_data_set_string(s_pos, "text", "POS\n");
		obs_data_set_string(s_name, "text", "EQUIPO\n");
		obs_data_set_string(s_res, "text", "RES\n");
		obs_data_set_string(s_time, "text", "TIEMPO\n");

		filter->sb_pos_source =
			obs_source_create_private("text_gdiplus_v2", name_pos, s_pos);
		if (!filter->sb_pos_source)
			filter->sb_pos_source =
				obs_source_create_private("text_gdiplus", name_pos, s_pos);

		filter->sb_name_source =
			obs_source_create_private("text_gdiplus_v2", name_name, s_name);
		if (!filter->sb_name_source)
			filter->sb_name_source =
				obs_source_create_private("text_gdiplus", name_name, s_name);

		filter->sb_solved_source =
			obs_source_create_private("text_gdiplus_v2", name_res, s_res);
		if (!filter->sb_solved_source)
			filter->sb_solved_source =
				obs_source_create_private("text_gdiplus", name_res, s_res);

		filter->sb_time_source =
			obs_source_create_private("text_gdiplus_v2", name_time, s_time);
		if (!filter->sb_time_source)
			filter->sb_time_source =
				obs_source_create_private("text_gdiplus", name_time, s_time);

		if (filter->sb_pos_source)
			obs_source_add_active_child(filter->source, filter->sb_pos_source);
		if (filter->sb_name_source)
			obs_source_add_active_child(filter->source, filter->sb_name_source);
		if (filter->sb_solved_source)
			obs_source_add_active_child(filter->source, filter->sb_solved_source);
		if (filter->sb_time_source)
			obs_source_add_active_child(filter->source, filter->sb_time_source);

		blog(LOG_INFO,
		     "[CUBE] Scoreboard columnas creadas (POS/EQUIPO/RES/TIEMPO)");

		obs_data_release(font_obj);
		obs_data_release(s_pos);
		obs_data_release(s_name);
		obs_data_release(s_res);
		obs_data_release(s_time);
	}

	/* Si salimos del modo 3, liberar columnas para evitar overlays residuales. */
	if (filter->mode != 3) {
		if (filter->sb_pos_source) {
			obs_source_remove_active_child(filter->source, filter->sb_pos_source);
			obs_source_release(filter->sb_pos_source);
			filter->sb_pos_source = NULL;
		}
		if (filter->sb_name_source) {
			obs_source_remove_active_child(filter->source, filter->sb_name_source);
			obs_source_release(filter->sb_name_source);
			filter->sb_name_source = NULL;
		}
		if (filter->sb_solved_source) {
			obs_source_remove_active_child(filter->source, filter->sb_solved_source);
			obs_source_release(filter->sb_solved_source);
			filter->sb_solved_source = NULL;
		}
		if (filter->sb_time_source) {
			obs_source_remove_active_child(filter->source, filter->sb_time_source);
			obs_source_release(filter->sb_time_source);
			filter->sb_time_source = NULL;
		}
	}

	if (filter->scoreboard_text_source) {
		obs_data_t *t_set = obs_data_create();
		obs_data_t *font_obj = obs_data_create();

		/* Si el tamaÃ±o es 0 o muy pequeÃ±o, usar un default de 25 */
		int fs = filter->scoreboard_font_size;
		if (fs <= 0) fs = 25;

		obs_data_set_int(font_obj, "size", fs);
		if (filter->scoreboard_font_face && filter->scoreboard_font_face[0])
			obs_data_set_string(font_obj, "face", filter->scoreboard_font_face);
		else
			obs_data_set_string(font_obj, "face", "Arial");
		
		obs_data_set_obj(t_set, "font", font_obj);
		obs_data_set_int(t_set, "color", filter->scoreboard_text_color);
		obs_data_set_int(t_set, "outline_color", filter->scoreboard_outline_color);
		obs_data_set_int(t_set, "outline_size", filter->scoreboard_outline_size);
		
		obs_source_update(filter->scoreboard_text_source, t_set);

		obs_data_release(font_obj);
		obs_data_release(t_set);
	}

	if (filter->sb_pos_source || filter->sb_name_source ||
	    filter->sb_solved_source || filter->sb_time_source) {
		obs_data_t *font_obj = obs_data_create();
		const int fs = (filter->scoreboard_font_size > 0) ? filter->scoreboard_font_size : 25;
		const int name_chars = clampi(filter->scoreboard_name_max_chars, 8, 60);
		const int w_pos = fs * 3 + 18;
		const int w_name = (int)((float)fs * (float)name_chars * 0.65f) + 24;
		const int w_res = fs * 4 + 18;
		const int w_time = fs * 6 + 22;

		obs_data_set_int(font_obj, "size", fs);
		if (filter->scoreboard_font_face && filter->scoreboard_font_face[0])
			obs_data_set_string(font_obj, "face", filter->scoreboard_font_face);
		else
			obs_data_set_string(font_obj, "face", "Arial");

		obs_source_t *cols[4] = {
			filter->sb_pos_source,
			filter->sb_name_source,
			filter->sb_solved_source,
			filter->sb_time_source
		};
		const int aligns[4] = {2, 0, 2, 2};
		const int widths[4] = {w_pos, w_name, w_res, w_time};

		for (int i = 0; i < 4; i++) {
			if (!cols[i])
				continue;
			obs_data_t *s = obs_source_get_settings(cols[i]);
			obs_data_set_obj(s, "font", font_obj);
			obs_data_set_int(s, "color", filter->scoreboard_text_color);
			obs_data_set_bool(s, "outline", true);
			obs_data_set_int(s, "outline_color", filter->scoreboard_outline_color);
			obs_data_set_int(s, "outline_size", filter->scoreboard_outline_size);
			obs_data_set_int(s, "align", aligns[i]);
			obs_data_set_bool(s, "extents", false);
			obs_data_set_int(s, "extents_cx", widths[i]);
			obs_data_set_int(s, "extents_cy", 0);
			obs_source_update(cols[i], s);
			obs_data_release(s);
		}

		obs_data_release(font_obj);
	}


	if (filter->sync_enabled) {
		bool use_domjudge = (filter->api_base_url && filter->api_base_url[0] &&
				     filter->contest_id && filter->contest_id[0]);

		if (use_domjudge) {
			blog(LOG_INFO, "[CUBE] Modo DOMjudge: %s/contests/%s",
			     filter->api_base_url, filter->contest_id);
			if (filter->web_sync) {
				web_sync_set_contest(filter->web_sync,
						     filter->api_base_url,
						     filter->contest_id);
				web_sync_set_interval(filter->web_sync,
						      filter->sync_interval_sec);
				web_sync_set_enabled(filter->web_sync, true);
			} else {
				filter->web_sync = web_sync_create_domjudge(
					filter->api_base_url,
					filter->contest_id,
					filter->sync_interval_sec);
			}
			if (filter->web_sync) {
				web_sync_set_auth(filter->web_sync, filter->api_username, filter->api_password);
			}
		} else {
			if (filter->web_sync)
				web_sync_set_enabled(filter->web_sync, false);
		}
	} else {
		if (filter->web_sync)
			web_sync_set_enabled(filter->web_sync, false);
	}
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
	filter->scale = (float)obs_data_get_double(settings, "scale");
	filter->rotation_x =(float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y =(float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z =(float)obs_data_get_double(settings, "rotation_z_slider_value");

	filter->ar_offset_pos_x =(float)obs_data_get_double(settings, "ar_offset_pos_x");
	filter->ar_offset_pos_y =(float)obs_data_get_double(settings, "ar_offset_pos_y");
	filter->ar_offset_pos_z =(float)obs_data_get_double(settings, "ar_offset_pos_z");
	filter->ar_offset_rot_x =(float)obs_data_get_double(settings, "ar_offset_rot_x");
	filter->ar_offset_rot_y =(float)obs_data_get_double(settings, "ar_offset_rot_y");
	filter->ar_offset_rot_z =(float)obs_data_get_double(settings, "ar_offset_rot_z");


	int id = (int)obs_data_get_int(settings, "marker_id");
	float size = (float)obs_data_get_double(settings, "marker_size");

	if (filter->detector) {
		if (size != get_marker_size(filter->detector))
			set_marker_size(filter->detector, size);
		if (id != get_marker_id(filter->detector))
			set_marker_id(filter->detector, id);
	}


	const char *new_model_path_c_str =
		obs_data_get_string(settings, "model_path");
	if (!new_model_path_c_str)
		new_model_path_c_str = "";

	if (!filter->model_path_str ||
	    strcmp(filter->model_path_str, new_model_path_c_str) != 0 ||
	    filter->g_mesh_count == 0) {

		bfree(filter->model_path_str);
		filter->model_path_str = bstrdup(new_model_path_c_str);

		if (filter->model_path_str && filter->model_path_str[0]) {
			blog(LOG_INFO, "[CUBE] Cargando nuevo modelo desde: %s",
			     filter->model_path_str);
			load_model_c(filter->model_path_str, &filter->g_meshes,
				     &filter->g_mesh_count,
				     &filter->model_width,
				     &filter->model_height);
		}
	}


	const char *new_texture_path_c_str =
		obs_data_get_string(settings, "texture_path");
	if (!new_texture_path_c_str)
		new_texture_path_c_str = "";

	bool model_present =
		(filter->g_meshes != NULL && filter->g_mesh_count > 0);

	bool texture_path_changed = false;
	if (!filter->texture_path_str)
		texture_path_changed = (new_texture_path_c_str[0] != '\0');
	else
		texture_path_changed = (strcmp(filter->texture_path_str,
					       new_texture_path_c_str) != 0);


	bool should_reload_texture = false;
	if (model_present) {
		if (texture_path_changed)
			should_reload_texture = true;
		else if (filter->loaded_texture == NULL &&
			 new_texture_path_c_str[0] != '\0')
			should_reload_texture = true;
	}

	if (should_reload_texture) {
		/* Mantengo exactamente la secuencia original para borrar/recargar */
		bfree(filter->texture_path_str);
		filter->texture_path_str = bstrdup(new_texture_path_c_str);

		/* Si existÃ­a una textura cargada, la destruyo (igual que en tu logica original). */
		if (filter->loaded_texture) {
			obs_enter_graphics();
			gs_texture_destroy(filter->loaded_texture);
			filter->loaded_texture = NULL;
			obs_leave_graphics();
		}

		if (filter->texture_path_str && filter->texture_path_str[0]) {
			filter->loaded_texture =
				load_texture_file(filter->texture_path_str);
			if (filter->loaded_texture) {
				blog(LOG_INFO,
				     "[CUBE] Nueva textura cargada desde: %s",
				     filter->texture_path_str);
				replace_mesh_textures(filter->g_meshes,
						      filter->g_mesh_count,
						      filter->loaded_texture,
						      NULL);
			} else {
				blog(LOG_WARNING,
				     "[CUBE] No se pudo cargar la nueva textura desde: %s",
				     filter->texture_path_str);
			}
		} else {
			/* ruta vacÃ­a: eliminar texturas en mallas */
			blog(LOG_INFO,
			     "[CUBE] Ruta de textura vacia: eliminando textura en mallas");
			replace_mesh_textures(filter->g_meshes,
					      filter->g_mesh_count, NULL, NULL);
		}
	} else {
	
		if (model_present && new_texture_path_c_str[0] == '\0' &&
		    filter->loaded_texture) {
			obs_enter_graphics();
			gs_texture_destroy(filter->loaded_texture);
			filter->loaded_texture = NULL;
			obs_leave_graphics();
			replace_mesh_textures(filter->g_meshes,
					      filter->g_mesh_count, NULL, NULL);
		}
	}

	
	int marker_dict = obs_data_get_int(settings, "marker_dict");
	if (filter->detector &&
	    marker_dict != get_marker_dictionary(filter->detector)) {
		set_marker_dictionary(filter->detector, marker_dict);
	}
}

/* Advances per-frame state, timers, and sync data. */
/* Avanza el estado por fotograma, los temporizadores y los datos de sincronizacion. */
static void filter_tick(void *data, float seconds)
{
	struct cube_filter_data *filter = data;
	struct obs_video_info video_info;

	if (obs_get_video_info(&video_info)) {
		if (video_info.base_width != filter->width_screen ||
		    video_info.base_height != filter->height_screen) {
			filter->width_screen = video_info.base_width;
			filter->height_screen = video_info.base_height;
			create_texture(filter);
		}
	}

	/* Modo Countdown o Scoreboard: actualizar reloj y sincronizacion web */
	if ((filter->mode == 2 || filter->mode == 3 || filter->mode == 4) && (filter->countdown_clock || filter->web_sync)) {
		if (filter->mode == 2 && filter->countdown_clock) {
			if (filter->countdown_reset_requested) {
				countdown_clock_reset(filter->countdown_clock);
				filter->countdown_reset_requested = false;
			}
			countdown_clock_tick(filter->countdown_clock, seconds);
		}
		if (filter->web_sync) {
			/* Actualizar info equipos cada vez para ambos modos  */
			if (filter->mode == 4 || filter->mode == 3) {
				filter->team_info_cache_count = web_sync_get_teams(filter->web_sync, filter->team_info_cache, 100);
			}

			web_sync_result_t sync_result;
			if (web_sync_poll(filter->web_sync, &sync_result) && sync_result.valid) {
				if (sync_result.contest_valid && filter->mode == 2) {
					countdown_clock_sync_full(filter->countdown_clock,
						sync_result.remaining_seconds, sync_result.total_duration);
				}

				/* Actualizar scoreboard si hay datos nuevos */
				if (filter->mode == 3 && sync_result.scoreboard_valid && sync_result.team_count > 0) {
					filter->scoreboard_team_count = sync_result.team_count;
					memcpy(filter->scoreboard_teams, sync_result.teams,
					       sizeof(scoreboard_team_t) * sync_result.team_count);

					const int max_rows = 10;
					const int row_count =
						(filter->scoreboard_team_count < max_rows)
							? filter->scoreboard_team_count
							: max_rows;

					filter->scoreboard_header_lines = 1;
					filter->scoreboard_row_count = row_count;
					filter->scoreboard_line_count =
						filter->scoreboard_header_lines + row_count;

					char col_pos[1024] = {0};
					char col_name[2048] = {0};
					char col_res[1024] = {0};
					char col_time[1024] = {0};

					int off_pos = 0, off_name = 0, off_res = 0, off_time = 0;
					off_pos += snprintf(col_pos + off_pos, sizeof(col_pos) - (size_t)off_pos, "POS\n");
					off_name += snprintf(col_name + off_name, sizeof(col_name) - (size_t)off_name, "EQUIPO\n");
					off_res += snprintf(col_res + off_res, sizeof(col_res) - (size_t)off_res, "RES\n");
					off_time += snprintf(col_time + off_time, sizeof(col_time) - (size_t)off_time, "TIEMPO\n");

					for (int i = 0; i < row_count; i++) {
						const scoreboard_team_t *t = &filter->scoreboard_teams[i];
						char team_clean[512] = {0};
						const char *team_name = t->team_name ? t->team_name : "";

						scoreboard_sanitize_name(team_name, team_clean, sizeof(team_clean));
						
						char team_trunc[512] = {0};
						utf8_truncate_with_ellipsis(team_clean, team_trunc, sizeof(team_trunc), filter->scoreboard_name_max_chars);

						char time_buf[32] = {0};
						int total_m = t->total_time;
						if (total_m < 0)
							total_m = 0;
						snprintf(time_buf, sizeof(time_buf), "%d", total_m);

						off_pos += snprintf(col_pos + off_pos, sizeof(col_pos) - (size_t)off_pos,
								    "%d\n", t->rank);
						off_name += snprintf(col_name + off_name, sizeof(col_name) - (size_t)off_name,
								     "%s\n", team_trunc);
						off_res += snprintf(col_res + off_res, sizeof(col_res) - (size_t)off_res,
								    "%d\n", t->num_solved);
						off_time += snprintf(col_time + off_time, sizeof(col_time) - (size_t)off_time,
								     "%s\n", time_buf);
					}

					if (filter->sb_pos_source) {
						obs_data_t *s = obs_source_get_settings(filter->sb_pos_source);
						obs_data_set_string(s, "text", col_pos);
						obs_source_update(filter->sb_pos_source, s);
						obs_data_release(s);
					}
					if (filter->sb_name_source) {
						obs_data_t *s = obs_source_get_settings(filter->sb_name_source);
						obs_data_set_string(s, "text", col_name);
						obs_source_update(filter->sb_name_source, s);
						obs_data_release(s);
					}
					if (filter->sb_solved_source) {
						obs_data_t *s = obs_source_get_settings(filter->sb_solved_source);
						obs_data_set_string(s, "text", col_res);
						obs_source_update(filter->sb_solved_source, s);
						obs_data_release(s);
					}
					if (filter->sb_time_source) {
						obs_data_t *s = obs_source_get_settings(filter->sb_time_source);
						obs_data_set_string(s, "text", col_time);
						obs_source_update(filter->sb_time_source, s);
						obs_data_release(s);
					}
				}
			}

			/* MODO 4: Team Info. Renderiza la info del equipo mapeado al marker actual */
			if (filter->mode == 4 && filter->scoreboard_text_source) {
				char ti_text[1024] = {0};
				int det_id = filter->team_info_detected_marker;

				static int log_throttle_ti = 0;
				bool do_log = (log_throttle_ti++ % 60 == 0);

				if (det_id >= 0) {
					const char *mapped_team_id =
						team_info_lookup_team_id(filter, det_id);

					if (mapped_team_id && mapped_team_id[0] != '\0') {
						scoreboard_team_t *found_team = NULL;
						for (int i = 0; i < filter->team_info_cache_count; i++) {
							if (strcmp(filter->team_info_cache[i].team_id, mapped_team_id) == 0) {
								found_team = &filter->team_info_cache[i];
								break;
							}
						}

						if (found_team) {
							char team_trunc[512] = {0};
							utf8_truncate_with_ellipsis(found_team->team_name, team_trunc, sizeof(team_trunc), filter->scoreboard_name_max_chars);

							snprintf(ti_text, sizeof(ti_text),
								"EQUIPO: %s\n"
								"PUESTO: %d\n"
								"RESUELTOS: %d",
								team_trunc,
								found_team->rank,
								found_team->num_solved);
							if (do_log) {
								blog(LOG_INFO, "[CUBE-TEAM-INFO] ArUco %d -> T_ID %s -> Equipo %s (Rk:%d)",
									det_id, mapped_team_id, team_trunc, found_team->rank);
							}
						} else {
							snprintf(ti_text, sizeof(ti_text),
								"Equipo %s no encontrado\n(en la cache; conectado?)",
								mapped_team_id);
							if (do_log) {
								blog(LOG_WARNING, "[CUBE-TEAM-INFO] ArUco %d -> T_ID %s (PERO NO ESTA EN CACHE)",
								     det_id, mapped_team_id);
							}
						}
					} else {
						snprintf(ti_text, sizeof(ti_text), "Marker %d detectado\n(Sin equipo asignado)", det_id);
						if (do_log) blog(LOG_INFO, "[CUBE-TEAM-INFO] ArUco %d SIN MAPEO", det_id);
					}
				} else {
					snprintf(ti_text, sizeof(ti_text), "Buscando ArUco marker...");
					if (do_log) blog(LOG_INFO, "[CUBE-TEAM-INFO] Sin marker en camara");
				}

				obs_data_t *txt_settings = obs_source_get_settings(filter->scoreboard_text_source);
				const char *old_text = obs_data_get_string(txt_settings, "text");
				if (!old_text || strcmp(old_text, ti_text) != 0) {
					obs_data_set_string(txt_settings, "text", ti_text);
					obs_source_update(filter->scoreboard_text_source, txt_settings);
				}
				obs_data_release(txt_settings);
			}
		}
	}
}

/* Persists the current filter configuration into OBS settings. */
/* Guarda la configuracion actual del filtro en los ajustes de OBS. */
static void filter_save(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	blog(LOG_INFO, "[CUBE] guardando valores");
	obs_data_set_double(settings, "pos_x", filter->pos_x);
	obs_data_set_double(settings, "pos_y", filter->pos_y);
	obs_data_set_double(settings, "pos_z", filter->pos_z);
	obs_data_set_double(settings, "scale", filter->scale);
	obs_data_set_double(settings, "rotation_x_slider_value", filter->rotation_x);
	obs_data_set_double(settings, "rotation_y_slider_value", filter->rotation_y);
	obs_data_set_double(settings, "rotation_z_slider_value", filter->rotation_z);
	obs_data_set_double(settings, "marker_size", get_marker_size(filter->detector));
	obs_data_set_int(settings, "marker_id", get_marker_id(filter->detector));
	obs_data_set_int(settings, "marker_dict", get_marker_dictionary(filter->detector));

	obs_data_set_double(settings, "ar_offset_pos_x", filter->ar_offset_pos_x);
	obs_data_set_double(settings, "ar_offset_pos_y", filter->ar_offset_pos_y);
	obs_data_set_double(settings, "ar_offset_pos_z", filter->ar_offset_pos_z);
	obs_data_set_double(settings, "ar_offset_rot_x", filter->ar_offset_rot_x);
	obs_data_set_double(settings, "ar_offset_rot_y", filter->ar_offset_rot_y);
	obs_data_set_double(settings, "ar_offset_rot_z", filter->ar_offset_rot_z);

	obs_data_set_int(settings, "countdown_duration_h", (int)filter->countdown_duration_h);
	obs_data_set_int(settings, "countdown_duration_m", (int)filter->countdown_duration_m);
	obs_data_set_int(settings, "countdown_duration_s", (int)filter->countdown_duration_s);
	obs_data_set_bool(settings, "countdown_running", filter->countdown_running);
	obs_data_set_bool(settings, "sync_enabled", filter->sync_enabled);
	obs_data_set_double(settings, "sync_interval_sec", (double)filter->sync_interval_sec);
	if (filter->api_base_url) obs_data_set_string(settings, "api_base_url", filter->api_base_url);
	if (filter->contest_id) obs_data_set_string(settings, "contest_id", filter->contest_id);
	if (filter->api_username) obs_data_set_string(settings, "api_username", filter->api_username);
	if (filter->api_password) obs_data_set_string(settings, "api_password", filter->api_password);
	obs_data_set_double(settings, "scoreboard_offset_x", (double)filter->scoreboard_offset_x);
	obs_data_set_double(settings, "scoreboard_offset_y", (double)filter->scoreboard_offset_y);
	obs_data_set_bool(settings, "scoreboard_centered", filter->scoreboard_centered);
	obs_data_set_int(settings, "scoreboard_font_size", filter->scoreboard_font_size);
	if (filter->scoreboard_font_face) obs_data_set_string(settings, "scoreboard_font_face", filter->scoreboard_font_face);
	obs_data_set_int(settings, "scoreboard_text_color", (int)filter->scoreboard_text_color);
	obs_data_set_int(settings, "scoreboard_outline_color", (int)filter->scoreboard_outline_color);
	obs_data_set_int(settings, "scoreboard_outline_size", filter->scoreboard_outline_size);
	obs_data_set_int(settings, "scoreboard_name_max_chars", filter->scoreboard_name_max_chars);
	obs_data_set_bool(settings, "scoreboard_row_stripes", filter->scoreboard_row_stripes);
	obs_data_set_int(settings, "scoreboard_row_stripe_opacity", filter->scoreboard_row_stripe_opacity);
	obs_data_set_bool(settings, "overlay_bg_enabled", filter->overlay_bg_enabled);
	obs_data_set_int(settings, "overlay_bg_color", (int)filter->overlay_bg_color);
	obs_data_set_int(settings, "overlay_bg_opacity", filter->overlay_bg_opacity);
	obs_data_set_int(settings, "overlay_bg_padding", filter->overlay_bg_padding);
	obs_data_set_int(settings, "overlay_bg_radius", filter->overlay_bg_radius);
	obs_data_set_bool(settings, "overlay_bg_shadow_enabled", filter->overlay_bg_shadow_enabled);
	obs_data_set_int(settings, "overlay_bg_shadow_opacity", filter->overlay_bg_shadow_opacity);
	obs_data_set_int(settings, "overlay_bg_shadow_offset_x", filter->overlay_bg_shadow_offset_x);
	obs_data_set_int(settings, "overlay_bg_shadow_offset_y", filter->overlay_bg_shadow_offset_y);
	obs_data_set_int(settings, "overlay_bg_shadow_softness", filter->overlay_bg_shadow_softness);
	obs_data_set_bool(settings, "overlay_ar_smooth_enabled", filter->overlay_ar_smooth_enabled);
	obs_data_set_double(settings, "overlay_ar_smooth_alpha", (double)filter->overlay_ar_smooth_alpha);

	/* Team Info: JSON local */
	obs_data_set_string(settings, "team_info_json_path",
			    filter->team_info_json_path ? filter->team_info_json_path
							: "");

	// Guardar configuracion de reloj
	obs_data_set_int(settings, "clock_mode", filter->clock_mode);
	//obs_data_set_int(settings, "mesh_id_dial", filter->mesh_id_dial);
	obs_data_set_int(settings, "mesh_id_hour_hand", filter->mesh_id_hour_hand);
	obs_data_set_int(settings, "mesh_id_minute_hand", filter->mesh_id_minute_hand);
	obs_data_set_int(settings, "mesh_id_second_hand", filter->mesh_id_second_hand);
	obs_data_set_int(settings, "mesh_id_single_hand", filter->mesh_id_single_hand);
	obs_data_set_bool(settings, "countdown_use_ar", filter->countdown_use_ar);

	if (filter->model_path_str) obs_data_set_string(settings, "model_path", filter->model_path_str);
	if (filter->texture_path_str) obs_data_set_string(settings, "texture_path", filter->texture_path_str);
	if (get_calibration_path(filter->detector)) obs_data_set_string(settings, "calibration_path", get_calibration_path(filter->detector));
}

/* Restores the filter configuration from OBS settings. */
/* Restaura la configuracion del filtro desde los ajustes de OBS. */
static void filter_load(void *data, obs_data_t *settings)
{
	struct cube_filter_data *filter = data;
	blog(LOG_INFO, "[CUBE] cargando valores");
	filter->pos_x = (float)obs_data_get_double(settings, "pos_x");
	filter->pos_y = (float)obs_data_get_double(settings, "pos_y");
	filter->pos_z = (float)obs_data_get_double(settings, "pos_z");
	filter->scale = (float)obs_data_get_double(settings, "scale");
	filter->rotation_x = (float)obs_data_get_double(settings, "rotation_x_slider_value");
	filter->rotation_y = (float)obs_data_get_double(settings, "rotation_y_slider_value");
	filter->rotation_z = (float)obs_data_get_double(settings, "rotation_z_slider_value");
	filter->ar_offset_pos_x = (float)obs_data_get_double(settings, "ar_offset_pos_x");
	filter->ar_offset_pos_y = (float)obs_data_get_double(settings, "ar_offset_pos_y");
	filter->ar_offset_pos_z = (float)obs_data_get_double(settings, "ar_offset_pos_z");
	filter->ar_offset_rot_x = (float)obs_data_get_double(settings, "ar_offset_rot_x");
	filter->ar_offset_rot_y = (float)obs_data_get_double(settings, "ar_offset_rot_y");
	filter->ar_offset_rot_z = (float)obs_data_get_double(settings, "ar_offset_rot_z");
	int id = (int)obs_data_get_int(settings, "marker_id");
	int dict = (int)obs_data_get_int(settings, "marker_dict");
	float siz = (float)obs_data_get_double(settings, "marker_size");
	if (id != get_marker_id(filter->detector)) set_marker_id(filter->detector, id);
	if (dict != get_marker_dictionary(filter->detector)) set_marker_dictionary(filter->detector, dict);
	if (siz != get_marker_size(filter->detector)) set_marker_size(filter->detector, siz);
	const char *mp = obs_data_get_string(settings, "model_path");
	if (filter->model_path_str) bfree(filter->model_path_str);
	filter->model_path_str = (mp && *mp) ? bstrdup(mp) : NULL;
	const char *tp = obs_data_get_string(settings, "texture_path");
	if (filter->texture_path_str) bfree(filter->texture_path_str);
	filter->texture_path_str = (tp && *tp) ? bstrdup(tp) : NULL;
	const char *cp = obs_data_get_string(settings, "calibration_path");
	if (cp && *cp) set_calibration_path(filter->detector, cp);

	filter->countdown_duration_h = (uint32_t)obs_data_get_int(settings, "countdown_duration_h");
	filter->countdown_duration_m = (uint32_t)obs_data_get_int(settings, "countdown_duration_m");
	filter->countdown_duration_s = (uint32_t)obs_data_get_int(settings, "countdown_duration_s");
	filter->countdown_running = obs_data_get_bool(settings, "countdown_running");
	filter->sync_enabled = obs_data_get_bool(settings, "sync_enabled");
	filter->sync_interval_sec = (float)obs_data_get_double(settings, "sync_interval_sec");

	bfree(filter->api_base_url);
	const char *abu = obs_data_get_string(settings, "api_base_url");
	filter->api_base_url = (abu && abu[0]) ? bstrdup(abu) : NULL;
	bfree(filter->contest_id);
	const char *cid = obs_data_get_string(settings, "contest_id");
	filter->contest_id = (cid && cid[0]) ? bstrdup(cid) : NULL;

	const char *au = obs_data_get_string(settings, "api_username");
	const char *ap = obs_data_get_string(settings, "api_password");
	bfree(filter->api_username);
	filter->api_username = (au && au[0]) ? bstrdup(au) : NULL;
	bfree(filter->api_password);
	filter->api_password = (ap && ap[0]) ? bstrdup(ap) : NULL;
	filter->scoreboard_offset_x = (float)obs_data_get_double(settings, "scoreboard_offset_x");
	filter->scoreboard_offset_y = (float)obs_data_get_double(settings, "scoreboard_offset_y");
	filter->scoreboard_centered = obs_data_get_bool(settings, "scoreboard_centered");
	filter->scoreboard_font_size = (int)obs_data_get_int(settings, "scoreboard_font_size");
	bfree(filter->scoreboard_font_face);
	const char *ff = obs_data_get_string(settings, "scoreboard_font_face");
	filter->scoreboard_font_face = (ff && ff[0]) ? bstrdup(ff) : NULL;
	filter->scoreboard_text_color = (uint32_t)obs_data_get_int(settings, "scoreboard_text_color");
	filter->scoreboard_outline_color = (uint32_t)obs_data_get_int(settings, "scoreboard_outline_color");
	filter->scoreboard_outline_size = (int)obs_data_get_int(settings, "scoreboard_outline_size");

	// Cargar configuracion de reloj
	filter->clock_mode = (int)obs_data_get_int(settings, "clock_mode");
	//filter->mesh_id_dial = (int)obs_data_get_int(settings, "mesh_id_dial");
	filter->mesh_id_hour_hand = (int)obs_data_get_int(settings, "mesh_id_hour_hand");
	filter->mesh_id_minute_hand = (int)obs_data_get_int(settings, "mesh_id_minute_hand");
	filter->mesh_id_second_hand = (int)obs_data_get_int(settings, "mesh_id_second_hand");
	filter->mesh_id_single_hand = (int)obs_data_get_int(settings, "mesh_id_single_hand");
	filter->countdown_use_ar = obs_data_get_bool(settings, "countdown_use_ar");

	filter_update(data, settings);
}
/* Sets the default values for all filter settings. */
/* Establece los valores predeterminados de todos los ajustes del filtro. */
static void filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "pos_x", 0.0);
	obs_data_set_default_double(settings, "pos_y", 0.0);
	obs_data_set_default_double(settings, "pos_z", 0.0);
	obs_data_set_default_double(settings, "scale", 100.0);
	obs_data_set_default_double(settings, "rotation_x_slider_value", 0.0);
	obs_data_set_default_double(settings, "rotation_y_slider_value", 0.0);
	obs_data_set_default_double(settings, "rotation_z_slider_value", 0.0);
	obs_data_set_default_double(settings, "marker_size", 0.1);
	obs_data_set_default_int(settings, "marker_id", 0);
	obs_data_set_default_string(settings, "model_path", "");
	obs_data_set_default_string(settings, "texture_path", "");
	obs_data_set_default_int(settings, "marker_dict", ARUCO_DICT_ORIGINAL);
	obs_data_set_default_string(settings, "calibration_path", "calibration.yml");


	obs_data_set_default_double(settings, "ar_offset_pos_x", 0.0);
	obs_data_set_default_double(settings, "ar_offset_pos_y", 0.0);
	obs_data_set_default_double(settings, "ar_offset_pos_z", 0.0);
	obs_data_set_default_double(settings, "ar_offset_rot_x", 0.0);
	obs_data_set_default_double(settings, "ar_offset_rot_y", 0.0);
	obs_data_set_default_double(settings, "ar_offset_rot_z", 0.0);

	obs_data_set_default_int(settings, "countdown_duration_h", 0);
	obs_data_set_default_int(settings, "countdown_duration_m", 5);
	obs_data_set_default_int(settings, "countdown_duration_s", 0);
	obs_data_set_default_bool(settings, "countdown_running", false);
	obs_data_set_default_bool(settings, "countdown_reset", false);
	obs_data_set_default_bool(settings, "sync_enabled", false);
	obs_data_set_default_double(settings, "sync_interval_sec", 10.0);
	obs_data_set_default_string(settings, "api_base_url", "");
	obs_data_set_default_string(settings, "contest_id", "");
	obs_data_set_default_double(settings, "scoreboard_offset_x", 10.0);
	obs_data_set_default_double(settings, "scoreboard_offset_y", 10.0);
	obs_data_set_default_bool(settings, "scoreboard_centered", false);
	obs_data_set_default_int(settings, "scoreboard_font_size", 24);
	obs_data_set_default_string(settings, "scoreboard_font_face", "Arial");
	obs_data_set_default_int(settings, "scoreboard_text_color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "scoreboard_outline_color", 0xFF000000);
	obs_data_set_default_int(settings, "scoreboard_outline_size", 2);
	obs_data_set_default_int(settings, "scoreboard_name_max_chars", 20);
	obs_data_set_default_bool(settings, "scoreboard_row_stripes", true);
	obs_data_set_default_int(settings, "scoreboard_row_stripe_opacity", 18);
	obs_data_set_default_bool(settings, "overlay_bg_enabled", true);
	obs_data_set_default_int(settings, "overlay_bg_color", 0x101010);
	obs_data_set_default_int(settings, "overlay_bg_opacity", 70);
	obs_data_set_default_int(settings, "overlay_bg_padding", 12);
	obs_data_set_default_int(settings, "overlay_bg_radius", 14);
	obs_data_set_default_bool(settings, "overlay_bg_shadow_enabled", true);
	obs_data_set_default_int(settings, "overlay_bg_shadow_opacity", 35);
	obs_data_set_default_int(settings, "overlay_bg_shadow_offset_x", 4);
	obs_data_set_default_int(settings, "overlay_bg_shadow_offset_y", 4);
	obs_data_set_default_int(settings, "overlay_bg_shadow_softness", 4);
	obs_data_set_default_bool(settings, "overlay_ar_smooth_enabled", true);
	obs_data_set_default_double(settings, "overlay_ar_smooth_alpha", 0.20);

	// Valores por defecto de configuracion de reloj
	obs_data_set_default_int(settings, "clock_mode", 0);  // Tres manecillas por defecto
	//obs_data_set_default_int(settings, "mesh_id_dial", 0);
	obs_data_set_default_int(settings, "mesh_id_hour_hand", 1);
	obs_data_set_default_int(settings, "mesh_id_minute_hand", 2);
	obs_data_set_default_int(settings, "mesh_id_second_hand", 3);
	obs_data_set_default_int(settings, "mesh_id_single_hand", 1);
	obs_data_set_default_bool(settings, "countdown_use_ar", false);

	/* Defaults for Team Info (JSON local) */
	obs_data_set_default_string(settings, "team_info_json_path", "");
}
static struct obs_source_info cube_filter = {
	.id = "cube_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = filter_get_name,
	.create = filter_create,
	.destroy = filter_destroy,
	.video_render = filter_render,
	.video_tick = filter_tick,
	.get_properties = filter_properties,
	.update = filter_update,
	.save = filter_save,
	.get_defaults = filter_defaults,
	.load = filter_load,
	.filter_video = filter_video,

};
bool obs_module_load(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	blog(LOG_INFO, "[CUBE] Registrando filtro");
	obs_register_source(&cube_filter);
	blog(LOG_INFO, "[CUBE] Registrade");
	return true;
}

void obs_module_unload(void)
{
	curl_global_cleanup();
}
