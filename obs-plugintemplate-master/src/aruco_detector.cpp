#include "aruco_detector.h"
#include <opencv2/aruco.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0f)
#endif

struct ArucoDetector {
	cv::Ptr<cv::aruco::Dictionary> dictionary;
	cv::Ptr<cv::aruco::DetectorParameters> detector_params;
	cv::Mat camera_matrix, dist_coeffs, mat_bgra;
	float marker_size;
	int id;
};

extern "C" {

ArucoDetector *initialize_aruco_detector(float marker_size_meters, int dict)
{
	auto *det = new ArucoDetector;
	set_marker_dictionary(det, dict);
	det->detector_params = cv::aruco::DetectorParameters::create();
	set_marker_size(det, marker_size_meters);
	// Cámara simulada
	det->id = 0;
	det->camera_matrix = (cv::Mat_<double>(3, 3) << 2000.0, 0.0, 1233.0,0.0, 2000.0, 718.0, 0.0, 0.0, 1.0);
	det->dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);
	return det;
}

void cleanup_aruco_detector(ArucoDetector *det)
{
	delete det;
}

// Convierte matriz de rotación 3×3 a ángulos Euler (pitch, yaw, roll)
static void rotation_to_euler(const cv::Mat &R, float &pitch, float &yaw,
			      float &roll)
{
	// Asumimos R de tipo CV_32F
	float r20 = R.at<float>(2, 0);
	if (std::abs(r20) > 0.999f) {
		// Gimbal lock
		pitch = (r20 < 0) ? M_PI_2 : -M_PI_2;
		yaw = std::atan2(-R.at<float>(0, 1), R.at<float>(0, 2));
		roll = 0.0f;
	} else {
		pitch = std::asin(-r20);
		yaw = std::atan2(R.at<float>(1, 0), R.at<float>(0, 0));
		roll = std::atan2(R.at<float>(2, 1), R.at<float>(2, 2));
	}
	// rad → deg
	pitch = pitch * 180.0f / M_PI;
	yaw = yaw * 180.0f / M_PI;
	roll = roll * 180.0f / M_PI;
}
/**
 * @brief Calcula los ángulos de Euler (pitch, yaw, roll) a partir de un vector de rotación y traslación.
 *
 * Este método es más robusto que la conversión manual, ya que utiliza la función
 * cv::decomposeProjectionMatrix de OpenCV.
 *
 * @param rvec Vector de rotación de 3x1.
 * @param tvec Vector de traslación de 3x1.
 * @param pitch Referencia para guardar el ángulo de pitch (rotación en X).
 * @param yaw Referencia para guardar el ángulo de yaw (rotación en Y).
 * @param roll Referencia para guardar el ángulo de roll (rotación en Z).
 */
static void get_euler_angles_from_pose(const cv::Vec3d &rvec,
				       const cv::Vec3d &tvec, float &pitch,
				       float &yaw, float &roll)
{
	// 1. Convertir el vector de rotación (rvec) a una matriz de rotación (R)
	cv::Mat R;
	cv::Rodrigues(rvec, R);

	// 2. Construir la matriz de proyección 3x4 [R | t]
	cv::Mat P;
	cv::hconcat(R, tvec, P); // P = [R | t]

	// 3. Descomponer la matriz de proyección para obtener los ángulos de Euler
	cv::Mat cameraMatrix, rotMatrix, transVect;
	cv::Vec3d eulerAngles;
	cv::decomposeProjectionMatrix(P, cameraMatrix, rotMatrix, transVect,cv::noArray(), cv::noArray(),cv::noArray(), eulerAngles);

	// 4. Asignar los valores a las variables de salida
	pitch = eulerAngles[0]; // Rotación alrededor del eje X
	yaw = eulerAngles[1];   // Rotación alrededor del eje Y
	roll = eulerAngles[2];  // Rotación alrededor del eje Z
}
bool process_frame_rgba(ArucoDetector *det, const uint8_t *frame_data, int w,int h, int fw, int fh, ArucoResult *res)
{
	if (!det || !frame_data || w <= 0 || h <= 0 || !res)
		return false;

	// 1) Copiar BGRA y convertir a gris
	det->mat_bgra.create(h, w, CV_8UC4);
	memcpy(det->mat_bgra.data, frame_data, size_t(w) * h * 4);
	cv::Mat gray;
	cv::cvtColor(det->mat_bgra, gray, cv::COLOR_BGRA2GRAY);

	// 2) Detectar marcadores en la imagen completa
	std::vector<std::vector<cv::Point2f>> corners;
	std::vector<int> ids;
	cv::aruco::detectMarkers(gray, det->dictionary, corners, ids,
				 det->detector_params);

	// 3) Si no se detecta NINGÚN marcador, salimos.
	if (ids.empty()) {
		res->detected = false;
		return false;
	}

	// 4) Buscar el índice del marcador cuyo ID coincida con det->id
	int marker_index_to_process = -1;
	for (int i = 0; i < ids.size(); ++i) {
		if (ids[i] == det->id) {
			marker_index_to_process = i;
			break;
		}
	}

	if (marker_index_to_process == -1) {
		// No se encontró el marcador con el ID deseado
		res->detected = false;
		return false;
	}

	// 5) Estimar la pose sólo del marcador seleccionado
	std::vector<cv::Vec3d> rvecs, tvecs;
	cv::aruco::estimatePoseSingleMarkers(corners, det->marker_size,
					     det->camera_matrix,
					     det->dist_coeffs, rvecs, tvecs);

	if (rvecs.empty() || tvecs.empty()) {
		res->detected = false;
		return false;
	}

	// 6) Rellenar resultado con el marcador encontrado
	res->detected = true;
	res->id = ids[marker_index_to_process];
	for (int i = 0; i < 3; ++i) {
		res->tvec[i] = float(tvecs[marker_index_to_process][i]);
		res->rvec[i] = float(rvecs[marker_index_to_process][i]);
	}

	// 7) Calcular centro y esquinas del marcador seleccionado
	float cx = 0.0f, cy = 0.0f;
	for (int i = 0; i < 4; ++i) {
		float x = corners[marker_index_to_process][i].x;
		float y = corners[marker_index_to_process][i].y;
		res->corners[i][0] = x;
		res->corners[i][1] = y;
		cx += x;
		cy += y;
	}
	cx /= 4.0f;
	cy /= 4.0f;

	// Ajuste "Aspect Fit" al tamaño del filtro OBS
	float scale_w = float(fw) / w, scale_h = float(fh) / h;
	float scale = std::min(scale_w, scale_h);
	float sw = w * scale, sh = h * scale;
	float ox = (fw - sw) * 0.5f, oy = (fh - sh) * 0.5f;
	res->screen_pos_x = cx * scale + ox;
	res->screen_pos_y = cy * scale + oy;

	// 8) Calcular ángulos de Euler con el método robusto
	float pitch, yaw, roll;
	get_euler_angles_from_pose(rvecs[marker_index_to_process], tvecs[marker_index_to_process], pitch, yaw,
 roll);
	res->euler_x = pitch;
	res->euler_y = yaw;
	res->euler_z = roll;

	return true;
}

 void set_marker_dictionary(ArucoDetector *det, int dict_id) {
	 cv::aruco::PREDEFINED_DICTIONARY_NAME cv_dict;
	 det->dictionary.release();
	 switch (dict_id) {
	 case ARUCO_DICT_ORIGINAL:
		 cv_dict = cv::aruco::DICT_ARUCO_ORIGINAL;
		 break;
	 case ARUCO_DICT_4X4_100:
		 cv_dict = cv::aruco::DICT_4X4_100;
		 break;
	 case ARUCO_DICT_5X5_100:
		 cv_dict = cv::aruco::DICT_5X5_100;
		 break;
	 case ARUCO_DICT_6X6_100:
		 cv_dict = cv::aruco::DICT_6X6_100;
		 break;
	 case ARUCO_DICT_7X7_100:
		 cv_dict = cv::aruco::DICT_7X7_100;
		 break;
	 case ARUCO_DICT_MIP_ORIGINAL:
		 cv_dict = cv::aruco::
			 DICT_ARUCO_ORIGINAL; // o el que corresponda a “MIP”
		 break;
	 default:
		 cv_dict = cv::aruco::DICT_4X4_100;
		 break;
	 }

	 det->dictionary = cv::aruco::getPredefinedDictionary(cv_dict);
 }

void set_marker_size(ArucoDetector *det, float size)
{
	det->marker_size = size;
}

 void set_marker_id(ArucoDetector *det,int id) {
	det->id = id;
}
} // extern "C"
