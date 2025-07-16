#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Parámetros y estado interno, opaque en C */
typedef struct ArucoDetector ArucoDetector;

/** Resultado de la detección */
typedef struct {
	bool detected;
	int id;
	float tvec[3];
	float rvec[3];
	float corners[4][2];
	float screen_pos_x;
	float screen_pos_y;
	float euler_x;
	float euler_y;
	float euler_z;
} ArucoResult;

/**
 * @brief Crea e inicializa un detector ArUco.
 * @param marker_size_meters  Tamaño (m) de tu marcador.
 * @return Puntero opaque (liberar con cleanup).
 */
ArucoDetector *initialize_aruco_detector(float marker_size_meters);

/** @brief Libera el detector */
void cleanup_aruco_detector(ArucoDetector *det);

/**
 * @brief Procesa un frame BGRA y rellena ArucoResult.
 * @return true si detectó al menos un marcador.
 */
bool process_frame_rgba(ArucoDetector *det, const uint8_t *frame_data,int width, int height, int filter_w, int filter_h,ArucoResult *result);


 void set_marker_size(ArucoDetector *det,float size);

 void set_marker_id(ArucoDetector *det,int id);

#ifdef __cplusplus
}
#endif
