#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARUCO_DICT_ORIGINAL 0
#define ARUCO_DICT_4X4_100 1
#define ARUCO_DICT_5X5_100 2
#define ARUCO_DICT_6X6_100 3
#define ARUCO_DICT_7X7_100 4
#define ARUCO_DICT_MIP_ORIGINAL 5

typedef struct ArucoDetector ArucoDetector;

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

// Initializes a new ArUco detector.
// Inicializa un nuevo detector ArUco.
ArucoDetector *initialize_aruco_detector(float marker_size_meters, int dict,
					 const char *calibration_path);

// Releases the ArUco detector resources.
// Libera los recursos del detector ArUco.
void cleanup_aruco_detector(ArucoDetector *det);

// Processes a frame for single-marker detection.
// Procesa un fotograma para deteccion de un solo marcador.
bool process_frame_rgba(ArucoDetector *det, struct obs_source_frame *frame,
			int base_w, int base_h, int fw, int fh,
			ArucoResult *res);

// Processes a frame for filtered multi-marker detection.
// Procesa un fotograma para deteccion filtrada de varios marcadores.
bool process_frame_rgba_select_ids(ArucoDetector *det,
				   struct obs_source_frame *frame, int base_w,
				   int base_h, int fw, int fh,
				   const int *allowed_ids,
				   size_t allowed_count, ArucoResult *res);

// Sets the active ArUco dictionary.
// Establece el diccionario ArUco activo.
void set_marker_dictionary(ArucoDetector *det, int dict_id);

// Sets the configured marker size.
// Establece el tamano configurado del marcador.
void set_marker_size(ArucoDetector *det, float size);

// Sets the configured marker ID.
// Establece el ID configurado del marcador.
void set_marker_id(ArucoDetector *det, int id);

// Loads camera calibration parameters from a file.
// Carga los parametros de calibracion de camara desde un archivo.
bool set_camera_calibration(ArucoDetector *det, const char *filename);

// Returns the active dictionary identifier.
// Devuelve el identificador del diccionario activo.
int get_marker_dictionary(const ArucoDetector *det);

// Returns the configured marker size.
// Devuelve el tamano configurado del marcador.
float get_marker_size(const ArucoDetector *det);

// Returns the configured marker ID.
// Devuelve el ID configurado del marcador.
int get_marker_id(const ArucoDetector *det);

// Sets the calibration file path.
// Establece la ruta del archivo de calibracion.
void set_calibration_path(ArucoDetector *det, const char *path);

// Returns the current calibration file path.
// Devuelve la ruta actual del archivo de calibracion.
const char *get_calibration_path(const ArucoDetector *det);

#ifdef __cplusplus
}
#endif
