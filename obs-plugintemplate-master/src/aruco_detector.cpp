#include "aruco_detector.h"
#include <opencv2/aruco.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <cmath>
#include <limits>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/aruco.hpp>
#include <obs-module.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0f)
#endif

// Estructura ArucoDetector (sin cambios)
struct ArucoDetector {
	cv::Ptr<cv::aruco::Dictionary> dictionary;
	cv::Ptr<cv::aruco::DetectorParameters> detector_params;
	cv::Mat camera_matrix, dist_coeffs, mat_bgra;
	float marker_size;
	int id;
	int marker_dict;
	std::string calibration_path;
};

// set_default_camera_params (sin cambios)
void set_default_camera_params(ArucoDetector *det)
{
	// Intrinsics por defecto: fx = fy = 2000, principal point (cx, cy) = (1233, 718)
	det->camera_matrix = (cv::Mat_<double>(3, 3) << 2000.0, 0.0, 1233.0,
			      0.0, 2000.0, 718.0, 0.0, 0.0, 1.0);

	// Distorsión por defecto: cero en todos los coeficientes
	det->dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);

	blog(LOG_WARNING, "[CUBE] Usando parámetros de cámara por defecto (fx=fy=2000, cx=1233, cy=718).");
}

// obs_frame_to_bgra (sin cambios)
cv::Mat obs_frame_to_bgra(struct obs_source_frame *frame)
{
	int width = frame->width;
	int height = frame->height;
	cv::Mat bgra; // Matriz de destino

	switch (frame->format) {
	case VIDEO_FORMAT_I420: {
		cv::Mat yuv_packed(height + height / 2, width, CV_8UC1);
		cv::Mat y_plane(height, width, CV_8UC1, frame->data[0],frame->linesize[0]);
		y_plane.copyTo(yuv_packed(cv::Rect(0, 0, width, height)));
		cv::Mat u_plane(height / 2, width / 2, CV_8UC1, frame->data[1],frame->linesize[1]);
		u_plane.copyTo(yuv_packed(cv::Rect(0, height, width / 2, height / 2)));
		cv::Mat v_plane(height / 2, width / 2, CV_8UC1, frame->data[2],frame->linesize[2]);
		v_plane.copyTo(yuv_packed(cv::Rect(width / 2, height, width / 2, height / 2)));
		cv::cvtColor(yuv_packed, bgra, cv::COLOR_YUV2BGRA_I420);
		break;
	}
	case VIDEO_FORMAT_I422: {
		cv::Mat y_plane(height, width, CV_8UC1, frame->data[0],frame->linesize[0]);
		cv::Mat u_plane(height, width / 2, CV_8UC1, frame->data[1],	frame->linesize[1]);
		cv::Mat v_plane(height, width / 2, CV_8UC1, frame->data[2],frame->linesize[2]);
		cv::Mat u_resized, v_resized;
		cv::resize(u_plane, u_resized, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
		cv::resize(v_plane, v_resized, cv::Size(width, height), 0, 0,cv::INTER_LINEAR);
		cv::Mat yuv_image;
		std::vector<cv::Mat> yuv_planes = {y_plane, u_resized, v_resized};
		cv::merge(yuv_planes, yuv_image);
		cv::Mat bgr;
		cv::cvtColor(yuv_image, bgr, cv::COLOR_YUV2BGR);
		cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);
		break;
	}
	case VIDEO_FORMAT_NV12: {
		cv::Mat nv12_packed(height + height / 2, width, CV_8UC1);
		cv::Mat y_plane(height, width, CV_8UC1, frame->data[0],
				frame->linesize[0]);
		cv::Mat uv_plane(height / 2, width, CV_8UC1, frame->data[1],
				 frame->linesize[1]);
		y_plane.copyTo(nv12_packed(cv::Rect(0, 0, width, height)));
		uv_plane.copyTo(
			nv12_packed(cv::Rect(0, height, width, height / 2)));
		cv::cvtColor(nv12_packed, bgra, cv::COLOR_YUV2BGRA_NV12);
		break;
	}
	case VIDEO_FORMAT_YUY2: {
		cv::Mat yuy2(height, width, CV_8UC2, frame->data[0],
			     frame->linesize[0]);
		cv::cvtColor(yuy2, bgra, cv::COLOR_YUV2BGRA_YUY2);
		break;
	}
	case VIDEO_FORMAT_UYVY: {
		cv::Mat uyvy(height, width, CV_8UC2, frame->data[0],
			     frame->linesize[0]);
		cv::cvtColor(uyvy, bgra, cv::COLOR_YUV2BGRA_UYVY);
		break;
	}
	case VIDEO_FORMAT_YVYU: {
		cv::Mat yvyu(height, width, CV_8UC2, frame->data[0],
			     frame->linesize[0]);
		cv::cvtColor(yvyu, bgra, cv::COLOR_YUV2BGRA_YVYU);
		break;
	}
	case VIDEO_FORMAT_BGRA: {
		bgra = cv::Mat(height, width, CV_8UC4, frame->data[0],
			       frame->linesize[0])
			       .clone();
		break;
	}
	case VIDEO_FORMAT_RGBA: {
		cv::Mat rgba(height, width, CV_8UC4, frame->data[0],
			     frame->linesize[0]);
		cv::cvtColor(rgba, bgra, cv::COLOR_RGBA2BGRA);
		break;
	}
	case VIDEO_FORMAT_BGR3:
	case VIDEO_FORMAT_BGRX: {
		cv::Mat bgr(height, width, CV_8UC3, frame->data[0],
			    frame->linesize[0]);
		cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);
		break;
	}
	case VIDEO_FORMAT_Y800: { // Grayscale
		cv::Mat gray(height, width, CV_8UC1, frame->data[0],
			     frame->linesize[0]);
		cv::cvtColor(gray, bgra, cv::COLOR_GRAY2BGRA);
		break;
	}
	default:
		blog(LOG_WARNING,
		     "[Aruco] Formato de video no soportado para conversión a BGRA: %s (%d)",
		     get_video_format_name(frame->format), frame->format);
		break;
	}

	if (bgra.empty()) {
		blog(LOG_WARNING,
		     "[Aruco] La conversión del frame a BGRA falló para el formato %s.",
		     get_video_format_name(frame->format));
	}

	return bgra;
}

extern "C" {
// load_camera_calibration (sin cambios)
bool load_camera_calibration(const std::string &filename,
			     cv::Mat &camera_matrix, cv::Mat &dist_coeffs)
{
	camera_matrix.release();
	dist_coeffs.release();
	cv::FileStorage fs(filename, cv::FileStorage::READ);
	if (!fs.isOpened()) {
		blog(LOG_ERROR,
		     "[CUBE] No se pudo abrir el archivo de calibración: %s",
		     filename.c_str());
		return false;
	}

	fs["camera_matrix"] >> camera_matrix;
	fs["distortion_coefficients"] >> dist_coeffs;

	if (camera_matrix.empty() || dist_coeffs.empty()) {
		blog(LOG_ERROR,
		     "[CUBE] Parámetros inválidos en el archivo de calibración.");
		return false;
	}

	blog(LOG_INFO, "[CUBE] Archivo de calibración cargado con éxito.");
	return true;
}

// set_camera_calibration (sin cambios)
bool set_camera_calibration(ArucoDetector *det, const char *filename)
{
	if (!det || !filename || *filename == '\0')
		return false;

	if (!load_camera_calibration(filename, det->camera_matrix,
				     det->dist_coeffs)) {
		blog(LOG_WARNING,
		     "[CUBE] Usando parámetros de cámara por defecto.");
		set_default_camera_params(det);
	} else {
		det->calibration_path = filename;
		blog(LOG_INFO,
		     "[CUBE] Calibración cargada y establecida desde %s",
		     filename);
	}

	return true;
}

// initialize_aruco_detector (sin cambios)
ArucoDetector *initialize_aruco_detector(float marker_size_meters, int dict,
					 const char *calibration_file)
{
	auto *det = new ArucoDetector;
	set_marker_dictionary(det, dict);
	det->detector_params = cv::aruco::DetectorParameters::create();
	set_marker_size(det, marker_size_meters);
	det->id = 0;
	blog(LOG_WARNING, "[CUBE] catgar cpsitas");
	if (!set_camera_calibration(det, calibration_file)) {
		blog(LOG_WARNING,
		     "[CUBE] Usando parámetros de cámara por defecto.");
		det->camera_matrix = (cv::Mat_<double>(3, 3) << 2000.0, 0.0,
				      1233.0, 0.0, 2000.0, 718.0, 0.0, 0.0,
				      1.0);
		det->dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);
	}
	return det;
}

// cleanup_aruco_detector (sin cambios)
void cleanup_aruco_detector(ArucoDetector *det)
{
	det->detector_params.release();
	det->camera_matrix.release();
	det->dictionary.release();
	det->camera_matrix.release();
	det->dist_coeffs.release();
	det->mat_bgra.release();
	delete det;
}


static void rotation_to_euler(const cv::Mat &R, float &pitch, float &yaw,
			      float &roll)
{
	
	double r20 = R.at<double>(2, 0);

	if (std::abs(r20) > 0.999) {
		
		pitch = (r20 < 0.0) ? M_PI_2 : -M_PI_2;
		yaw = std::atan2(-R.at<double>(0, 1), R.at<double>(0, 2));
		roll = 0.0;
	} else {
		pitch = std::asin(-r20);
		yaw = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0));
		roll = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2));
	}

	
	pitch = static_cast<float>(pitch * 180.0 / M_PI);
	yaw = static_cast<float>(yaw * 180.0 / M_PI);
	roll = static_cast<float>(roll * 180.0 / M_PI);
}


bool process_frame_rgba(ArucoDetector *det, struct obs_source_frame *frame,
			int base_w, int base_h, int fw, int fh,
			ArucoResult *res)
{
	if (!det || !frame || !res)
		return false;

	int w = base_w; // ancho base de referencia
	int h = base_h; // alto base de referencia

	// Convertir frame a BGRA
	cv::Mat bgra = obs_frame_to_bgra(frame);
	if (bgra.empty()) {
		blog(LOG_WARNING,
		     "process_frame_rgba_scaled: conversión a BGRA falló");
		return false;
	}

	// Convertir a gris
	cv::Mat gray;
	cv::cvtColor(bgra, gray, cv::COLOR_BGRA2GRAY);

	// Detectar marcadores
	std::vector<std::vector<cv::Point2f>> corners;
	std::vector<int> ids;
	cv::aruco::detectMarkers(gray, det->dictionary, corners, ids,
				 det->detector_params);

	if (ids.empty()) {
		res->detected = false;
		return false;
	}

	// Buscar marcador de interés
	int marker_index = -1;
	for (int i = 0; i < (int)ids.size(); ++i) {
		if (ids[i] == det->id) {
			marker_index = i;
			break;
		}
	}
	if (marker_index == -1) {
		res->detected = false;
		return false;
	}

	// Estimar pose
	std::vector<cv::Vec3d> rvecs, tvecs;
	cv::aruco::estimatePoseSingleMarkers(corners, det->marker_size,
					     det->camera_matrix,
					     det->dist_coeffs, rvecs, tvecs);
	if (rvecs.empty() || tvecs.empty()) {
		res->detected = false;
		return false;
	}

	res->detected = true;
	res->id = ids[marker_index];
	for (int i = 0; i < 3; ++i) {
		res->tvec[i] = float(tvecs[marker_index][i]);
		res->rvec[i] = float(rvecs[marker_index][i]);
	}

	// Centro del marcador en coords base
	float im_cx = 0.0f, im_cy = 0.0f;
	for (int i = 0; i < 4; ++i) {
		float vx = corners[marker_index][i].x * ((float)base_w / frame->width);
		float vy = corners[marker_index][i].y * ((float)base_h / frame->height);

		vx = std::clamp(vx, 0.0f, float(w - 1));
		vy = std::clamp(vy, 0.0f, float(h - 1));

		res->corners[i][0] = vx;
		res->corners[i][1] = vy;

		im_cx += vx;
		im_cy += vy;
	}
	im_cx /= 4.0f;
	im_cy /= 4.0f;

	// Escalar al tamaño de salida OBS
	double scale_x = double(fw) / double(base_w);
	double scale_y = double(fh) / double(base_h);

	res->screen_pos_x = float(im_cx * scale_x);
	res->screen_pos_y = float(im_cy * scale_y);

	res->screen_pos_x = std::clamp(res->screen_pos_x, 0.0f, float(fw));
	res->screen_pos_y = std::clamp(res->screen_pos_y, 0.0f, float(fh));

	cv::Mat R;
	cv::Rodrigues(rvecs[marker_index], R);
	float pitch, yaw, roll;
	rotation_to_euler(R, pitch, yaw, roll);
	
	res->euler_x = pitch;
	res->euler_y = yaw;
	res->euler_z =-roll; // Mantenemos el -roll por si es un ajuste de ejes (OpenCV vs OBS)
	

	return true;
}

// set_marker_dictionary (sin cambios)
void set_marker_dictionary(ArucoDetector *det, int dict_id)
{
	if (!det)
		return;

	cv::aruco::PREDEFINED_DICTIONARY_NAME cv_dict;

	// Mapea el ID entero de OBS al enum de OpenCV
	switch (dict_id) {
	case ARUCO_DICT_ORIGINAL: // 9
		cv_dict = cv::aruco::DICT_ARUCO_ORIGINAL;
		break;
	case ARUCO_DICT_4X4_100: // 1
		cv_dict = cv::aruco::DICT_4X4_100;
		break;
	case ARUCO_DICT_5X5_100: // 5
		cv_dict = cv::aruco::DICT_5X5_100;
		break;
	case ARUCO_DICT_6X6_100: // 10
		cv_dict = cv::aruco::DICT_6X6_100;
		break;
	case ARUCO_DICT_7X7_100: // 14
		cv_dict = cv::aruco::DICT_7X7_100;
		break;
	case ARUCO_DICT_MIP_ORIGINAL: // 32
		cv_dict = cv::aruco::DICT_ARUCO_ORIGINAL;
		break;
	default:
		// Fallback seguro si el ID no es válido
		blog(LOG_WARNING,
		     "[Aruco] ID de diccionario desconocido: %d. Usando 4X4_100.",
		     dict_id);
		cv_dict = cv::aruco::DICT_4X4_100;
		dict_id =
			ARUCO_DICT_4X4_100; 
		break;
	}

	
	det->dictionary = cv::aruco::getPredefinedDictionary(cv_dict);


	det->marker_dict = dict_id;

	blog(LOG_INFO, "[Aruco] Diccionario de marcadores cambiado a: %d",  dict_id);
}


void set_marker_size(ArucoDetector *const det, float size)
{
	det->marker_size = size;
}


void set_marker_id(ArucoDetector *const det, int id)
{
	det->id = id;
}


const int get_marker_dictionary(const ArucoDetector *const det)
{
	return det->marker_dict;
}


const int get_marker_size(const ArucoDetector *const det)
{
	return det->marker_size;
}


const int get_marker_id(const ArucoDetector *const det)
{
	return det->id;
}


const char *get_calibration_path(const ArucoDetector *const det)
{
	return det ? det->calibration_path.c_str() : NULL;
}


void set_calibration_path(ArucoDetector *det, const char *const path)
{
	if (det && path &&
	    load_camera_calibration(path, det->camera_matrix,
				    det->dist_coeffs)) {
		det->calibration_path = path;

	} else
		set_default_camera_params(det);
}

} // extern "C"