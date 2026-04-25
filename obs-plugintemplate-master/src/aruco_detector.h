#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Valores puros C para elegir el diccionario
#define ARUCO_DICT_ORIGINAL 0
#define ARUCO_DICT_4X4_100 1
#define ARUCO_DICT_5X5_100 2
#define ARUCO_DICT_6X6_100 3
#define ARUCO_DICT_7X7_100 4
#define ARUCO_DICT_MIP_ORIGINAL 5

/** Parametros y estado interno, opaque en C */
typedef struct ArucoDetector ArucoDetector;

/** Resultado de la deteccion */
typedef struct {
	bool detected;
	int id;
	float tvec[3];
	float rvec[3];
	float corners[4][2];
	float screen_pos_x;
	float screen_pos_y;
	float normal_tip_x;
	float normal_tip_y;
	bool normal_tip_valid;
	float euler_x;
	float euler_y;
	float euler_z;
} ArucoResult;

/**
 * @brief Crea e inicializa un detector ArUco.
 * @param marker_size_meters  Tamao (m) de tu marcador.
 * @return Puntero opaque (liberar con cleanup).
 */
ArucoDetector *initialize_aruco_detector(float marker_size_meters, int dict, const char *calibration_file);

/** @brief Libera el detector */
void cleanup_aruco_detector(ArucoDetector *det);

/**
 * @brief Procesa un frame BGRA y rellena ArucoResult.
 * @return true si detect al menos un marcador.
 */
bool process_frame_rgba(ArucoDetector *det, struct obs_source_frame *frame, int base_w, int base_h, int fw, int fh, ArucoResult *res);

/**
 * @brief Procesa un frame y selecciona un marcador entre una lista permitida.
 *
 * Se usa para el modo Team Info, donde el JSON define qué IDs de marcador son válidos.
 * No bloquea el hilo de render: esta función debe llamarse desde el camino de vídeo (filter_video).
 *
 * @param allowed_ids     Lista de IDs de marcador a buscar (por ejemplo, los del JSON).
 * @param allowed_count   Número de IDs en la lista.
 * @return true si se detecta al menos un marcador permitido.
 */
bool process_frame_rgba_select_ids(ArucoDetector *det, struct obs_source_frame *frame,
				  int base_w, int base_h, int fw, int fh,
				  const int *allowed_ids, size_t allowed_count,
				  ArucoResult *res);

void set_marker_dictionary(ArucoDetector *const det, int dict_id);
void set_marker_size(ArucoDetector *const det, float size);
void set_marker_id(ArucoDetector *const det, int id);
bool set_camera_calibration(ArucoDetector *det, const char *filename);
const int get_marker_dictionary(const ArucoDetector *const det);
const int get_marker_size(const ArucoDetector *const det);
const int get_marker_id(const ArucoDetector *const det);
void set_calibration_path(ArucoDetector *det, const char *const path);
const char *get_calibration_path(const ArucoDetector *const det);

#ifdef __cplusplus
}
#endif
