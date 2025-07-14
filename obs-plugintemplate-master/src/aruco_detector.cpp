#include <cstring>
#include <opencv2/aruco.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <obs.h>
#include "aruco_detector.h"
#include <direct.h> // Para _getcwd en Windows
#include <opencv2/calib3d.hpp>
#include <cmath>  // Para std::sqrt, std::cos, std::sin, std::atan2, std::asin
#include <limits> // Para std::numeric_limits

// Define M_PI si no está disponible (algunos compiladores pueden requerir _USE_MATH_DEFINES)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Define M_PI_2 si no está disponible
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923f // M_PI / 2
#endif

// Variables globales (inicializadas en initialize_aruco_detector)
static cv::Ptr<cv::aruco::Dictionary> dictionary;
static cv::Ptr<cv::aruco::DetectorParameters> detector_params;
static cv::Mat camera_matrix, dist_coeffs;
static float marker_size_meters = 0.1f;

// Matrices reutilizables
static cv::Mat mat_bgra, mat_bgr;

void initialize_aruco_detector()
{
	blog(LOG_INFO, "initialize_aruco_detector: inicializando");

	// Diccionario ArUco, 4x4 con 100 marcadores
	dictionary =
		cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_100);

	// Parámetros del detector con valores por defecto
	detector_params = cv::aruco::DetectorParameters::create();

	// Cámara genérica: centro óptico en el centro exacto del frame (resolución 2466x1436)
	// Focal (fx, fy) puestas arbitrariamente en 2000, ajusta si tienes calibración real
	// cx = ancho/2 = 2466/2 = 1233
	// cy = alto/2 = 1436/2 = 718
	camera_matrix = (cv::Mat_<double>(3, 3) << 2000.0, 0.0, 1233.0, 0.0,
			 2000.0, 718.0, 0.0, 0.0, 1.0);

	// Coeficientes de distorsión nulos (sin distorsión)
	dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);

	blog(LOG_INFO, "initialize_aruco_detector: completado");
}

void cleanup_aruco_detector()
{
	blog(LOG_INFO, "cleanup_aruco_detector: limpiando recursos");
	dictionary.release();
	detector_params.release();
	camera_matrix.release();
	dist_coeffs.release();
	blog(LOG_INFO, "cleanup_aruco_detector: recursos liberados");
}

/**
 * @brief Convierte un vector de rotación (Rodrigues) a una matriz de rotación 3x3.
 * @param rvec Vector de rotación (eje-ángulo, 3 elementos).
 * @param R Matriz de rotación 3x3 de salida.
 *
 * Implementa la fórmula de Rodrigues para convertir un vector de rotación (eje y ángulo)
 * en una matriz de rotación.
 */
void rvec_to_rotation_matrix(const float rvec[3], float R[3][3])
{
	// Calcula la magnitud del vector de rotación, que es el ángulo theta.
	float theta = std::sqrt(rvec[0] * rvec[0] + rvec[1] * rvec[1] +
				rvec[2] * rvec[2]);

	// Si el ángulo es muy pequeño, la rotación es insignificante, se usa la matriz identidad.
	if (theta <
	    std::numeric_limits<float>::
		    epsilon()) { // Usar epsilon para comparación de flotantes
		R[0][0] = 1;
		R[0][1] = 0;
		R[0][2] = 0;
		R[1][0] = 0;
		R[1][1] = 1;
		R[1][2] = 0;
		R[2][0] = 0;
		R[2][1] = 0;
		R[2][2] = 1;
		return;
	}

	// Normaliza el vector de rotación para obtener el eje unitario (kx, ky, kz).
	float kx = rvec[0] / theta;
	float ky = rvec[1] / theta;
	float kz = rvec[2] / theta;

	// Calcula los términos trigonométricos.
	float c = std::cos(theta); // cos(theta)
	float s = std::sin(theta); // sin(theta)
	float v = 1 - c;           // 1 - cos(theta)

	// Rellena la matriz de rotación usando la fórmula de Rodrigues.
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

/**
 * @brief Convierte una matriz de rotación 3x3 a ángulos de Euler (Yaw, Pitch, Roll).
 * @param R Matriz de rotación 3x3 de entrada.
 * @param out_pitch Ángulo de Pitch (rotación alrededor del eje Y), en grados.
 * @param out_yaw Ángulo de Yaw (rotación alrededor del eje Z), en grados.
 * @param out_roll Ángulo de Roll (rotación alrededor del eje X), en grados.
 *
 * Esta función extrae los ángulos de Euler (ZYX - Yaw, Pitch, Roll) de una matriz de rotación.
 * Maneja el caso de "gimbal lock" cuando el pitch es +/- 90 grados.
 */
void rotation_matrix_to_euler(const float R[3][3], float &out_pitch,float &out_yaw, float &out_roll)
{
	// Umbral para detectar gimbal lock (pitch cerca de +/- 90 grados)
	// R[2][0] = -sin(pitch)
	if (std::abs(R[2][0]) >= 1.0f - std::numeric_limits<float>::epsilon()) {
		// Gimbal lock: pitch = ±90°
		out_pitch = (R[2][0] < 0) ? M_PI_2 : -M_PI_2; // ← cambio aquí

		// Establecemos roll = 0, y calculamos yaw de forma aproximada
		out_yaw = std::atan2(-R[0][1], R[0][2]);
		out_roll = 0.0f;
	} else {
		out_pitch = -std::asin(R[2][0]);
		out_yaw = std::atan2(R[1][0], R[0][0]);
		out_roll = std::atan2(R[2][1], R[2][2]);
	}

	// Convertir a grados
	out_pitch *= 180.0f / M_PI;
	out_yaw *= 180.0f / M_PI;
	out_roll *= 180.0f / M_PI;
}

bool process_frame_rgba(const uint8_t *frame_data, int width, int height,
			int filter_width, int filter_height,
			ArucoResult *result)
{
	if (!frame_data || width <= 0 || height <= 0 || !dictionary) {
		blog(LOG_WARNING,
		     "process_frame_rgba: Datos inválidos o diccionario no inicializado");
		return false;
	}

	if (!result) {
		blog(LOG_WARNING, "process_frame_rgba: result es NULL");
		return false;
	}

	// Crear imagen BGRA
	mat_bgra.create(height, width, CV_8UC4);
	memcpy(mat_bgra.data, frame_data,static_cast<size_t>(width) * height * 4);

	// Convertir a escala de grises directamente
	cv::Mat mat_gray;
	cv::cvtColor(mat_bgra, mat_gray, cv::COLOR_BGRA2GRAY);

	// Redimensionar a la mitad para acelerar la detección
	cv::Mat mat_gray_resized;
	cv::resize(mat_gray, mat_gray_resized, cv::Size(), 0.5, 0.5);
	cv::imwrite("C:\\temp\\debug_gray_resized.png", mat_gray_resized);
	// Detectar marcadores en la imagen reducida
	std::vector<std::vector<cv::Point2f>> corners;
	std::vector<int> ids;
	cv::aruco::detectMarkers(mat_gray_resized, dictionary, corners, ids,detector_params);

	if (ids.empty()) {
		blog(LOG_INFO, "process_frame_rgba: No se detectaron marcadores");
		result->detected = false;
		return false;
	}

	// Estimar pose de los marcadores detectados
	// rvecs y tvecs representan la transformación del sistema de coordenadas del marcador
	// al sistema de coordenadas de la cámara.
	std::vector<cv::Vec3d> rvecs, tvecs;
	cv::aruco::estimatePoseSingleMarkers(corners, marker_size_meters, camera_matrix, dist_coeffs, rvecs, tvecs);

	if (rvecs.empty() || tvecs.empty()) {
		blog(LOG_WARNING,
		     "process_frame_rgba: estimación de pose falló");
		result->detected = false;
		return false;
	}

	// Rellenar resultados con datos del primer marcador detectado
	result->detected = true;
	result->id = ids[0];

	for (int i = 0; i < 3; ++i) {
		result->tvec[i] = static_cast<float>(tvecs[0][i]);
		result->rvec[i] = static_cast<float>(rvecs[0][i]);
	}

	// Obtener las esquinas reales del marcador detectado (escaladas a la resolución original)
	for (int i = 0; i < 4; ++i) {
		result->corners[i][0] =corners[0][i].x *2.0f; // Escalar por 2.0 porque se redimensionó a 0.5
		result->corners[i][1] = corners[0][i].y * 2.0f;
	}

	// Calcular centro en coordenadas originales
	float cx = 0, cy = 0;
	for (int i = 0; i < 4; ++i) {
		cx += result->corners[i][0];
		cy += result->corners[i][1];
	}
	cx /= 4.0f;
	cy /= 4.0f;

	// Convertir a espacio del filtro de OBS (si es diferente al tamaño del frame)
	result->screen_pos_x = cx;
	result->screen_pos_y = cy;

	// Convierte rvec a matriz de rotación y luego a ángulos de Euler
	// Convertir rvec[3] a cv::Mat para usar con Rodrigues
	cv::Mat rvec_cv(3, 1, CV_32F, (void *)result->rvec);
	cv::Mat R_cv;

	// Convertir rvec a matriz de rotación 3x3
	cv::Rodrigues(rvec_cv, R_cv);

	// Convertir R_cv (cv::Mat 3x3) a float R[3][3] para usar con tu función
	float R[3][3];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			R[i][j] = static_cast<float>(R_cv.at<float>(i, j));
		}
	}

	// Convertir matriz de rotación a ángulos de Euler
	float pitch_rad, yaw_rad, roll_rad;
	rotation_matrix_to_euler(R, pitch_rad, yaw_rad, roll_rad);

	// Guardar los ángulos en el resultado
	result->euler_x = pitch_rad;
	result->euler_y = yaw_rad;
	result->euler_z = roll_rad;


	blog(LOG_INFO, "Screen position: (%.2f, %.2f)", result->screen_pos_x,result->screen_pos_y);
	blog(LOG_INFO, "Euler angles (deg): Pitch=%.2f, Yaw=%.2f, Roll=%.2f", result->euler_x, result->euler_y, result->euler_z);
	return true;
}
