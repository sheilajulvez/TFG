#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Resultado de detección de un marcador ArUco */
typedef struct {
	bool detected;
	int id;
	float tvec[3];
	float rvec[3];
	float corners[4][2];
	float screen_pos_x; // Centro en pantalla X
	float screen_pos_y; // Centro en pantalla Y
	float euler_x;      // Rotación en grados X (pitch)
	float euler_y;      // Rotación en grados Y (yaw)
	float euler_z;      // Rotación en grados Z (roll)
} ArucoResult;

/**
 * @brief Inicializa el detector ArUco (diccionario, parámetros, cámara virtual).
 */
void initialize_aruco_detector(void);

/**
 * @brief Libera los recursos del detector ArUco.
 */
void cleanup_aruco_detector(void);

/**
 * @brief Procesa un frame BGRA y detecta marcadores ArUco.
 * 
 * @param frame_data   Puntero al buffer de imagen (BGRA, 8 bits)
 * @param width        Ancho del frame
 * @param height       Alto del frame
 * @param result       Puntero a la estructura donde se guarda el resultado
 * @return true si se detectó algún marcador, false si no
 */
bool process_frame_rgba(const unsigned char *frame_data, int width, int height,
			int filter_width, int filter_height,
			ArucoResult *result);

#ifdef __cplusplus
}
#endif
