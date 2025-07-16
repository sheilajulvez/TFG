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

ArucoDetector *initialize_aruco_detector(float marker_size_meters)
{
	auto *det = new ArucoDetector;
	det->dictionary =cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_100);
	det->detector_params = cv::aruco::DetectorParameters::create();
	det->marker_size = marker_size_meters;
	// Cámara simulada
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

bool process_frame_rgba(ArucoDetector *det, const uint8_t *frame_data, int w,
			int h, int fw, int fh, ArucoResult *res)
{
	if (!det || !frame_data || w <= 0 || h <= 0 || !res)
		return false;

	// 1) Copiar BGRA
	det->mat_bgra.create(h, w, CV_8UC4);
	memcpy(det->mat_bgra.data, frame_data, size_t(w) * h * 4);

	// 2) Gris y reducida
	cv::Mat gray, small;
	cv::cvtColor(det->mat_bgra, gray, cv::COLOR_BGRA2GRAY);
	cv::resize(gray, small, {}, 0.5, 0.5);

	// 3) Detectar esquinas
	std::vector<std::vector<cv::Point2f>> corners;
	std::vector<int> ids;
	cv::aruco::detectMarkers(small, det->dictionary, corners, ids,det->detector_params);
	if (ids.empty() || corners.empty() || corners[0].size() != 4) {
		res->detected = false;
		return false;
	}

	// 4) Estimar pose
	std::vector<cv::Vec3d> rvecs, tvecs;
	cv::aruco::estimatePoseSingleMarkers(corners, det->marker_size,
					     det->camera_matrix,
					     det->dist_coeffs, rvecs, tvecs);
	if (rvecs.empty() || tvecs.empty()) {
		res->detected = false;
		return false;
	}

	// 5) Rellenar resultado
	res->detected = true;
	bool match_found = false;
	int matched_index = -1;

	for (int i = 0; i < ids.size(); ++i) {
		int detected_id = ids[i];

		if (detected_id ==det->id) {
			matched_index = i;
			match_found = true;
			break;
		}
	}

	if (!match_found) {
		res->detected = false;
		return false;
	}
	for (int i = 0; i < 3; ++i) {
		res->tvec[i] = float(tvecs[0][i]);
		res->rvec[i] = float(rvecs[0][i]);
	}

	// 6) Calcular centro y esquinas en resolución original
	float cx = 0.0f, cy = 0.0f;
	for (int i = 0; i < 4; ++i) {
		float x = corners[0][i].x * 2.0f;
		float y = corners[0][i].y * 2.0f;
		res->corners[i][0] = x;
		res->corners[i][1] = y;
		cx += x;
		cy += y;
	}
	cx /= 4.0f;
	cy /= 4.0f;

	// Ajuste “Aspect Fit” al tamaño del filtro OBS
	float scale_w = float(fw) / w, scale_h = float(fh) / h;
	float scale = std::min(scale_w, scale_h);
	float sw = w * scale, sh = h * scale;
	float ox = (fw - sw) * 0.5f, oy = (fh - sh) * 0.5f;
	res->screen_pos_x = cx * scale + ox;
	res->screen_pos_y = cy * scale + oy;

	// 7) de rvec → R → Euler
	cv::Mat R;
	cv::Rodrigues(rvecs[0], R);
	if (R.type() != CV_32F)R.convertTo(R, CV_32F);
	float pitch, yaw, roll;
	rotation_to_euler(R, pitch, yaw, roll);
	res->euler_x = pitch;
	res->euler_y = yaw;
	res->euler_z = roll;

	return true;
}
 void set_marker_size(ArucoDetector *det,float size) {
	det->marker_size = size;
}

 void set_marker_id(ArucoDetector *det,int id) {
	det->id = id;
}
} // extern "C"
