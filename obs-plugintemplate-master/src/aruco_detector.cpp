#include "aruco_detector.h"
#include <opencv2/aruco.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <algorithm>
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

/**
 * @brief Sets default camera intrinsic and distortion parameters.
 *
 * This function initializes the camera matrix with default focal lengths (fx, fy = 2000)
 * and a default principal point (cx = 1233, cy = 718). The distortion coefficients
 * are set to zero. These values are used as a fallback when a calibration file is
 * not available or fails to load.
 *
 * @param det Pointer to the ArucoDetector structure to modify.
 */
// set_default_camera_params (sin cambios)
void set_default_camera_params(ArucoDetector *det)
{
	// Intrinsics por defecto: fx = fy = 2000, principal point (cx, cy) = (1233, 718)
	det->camera_matrix = (cv::Mat_<double>(3, 3) << 2000.0, 0.0, 1233.0,
			      0.0, 2000.0, 718.0, 0.0, 0.0, 1.0);

	// Distorsion por defecto: cero en todos los coeficientes
	det->dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);

	blog(LOG_WARNING, "[CUBE] Usando parametros de camara por defecto (fx=fy=2000, cx=1233, cy=718).");
}

/**
 * @brief Converts an OBS video frame from various YUV/RGB formats to BGRA.
 *
 * This function handles the conversion of an `obs_source_frame` into a `cv::Mat`
 * with a BGRA color format, which is required for processing with OpenCV. It supports
 * multiple video formats, including I420, I422, NV12, YUY2, UYVY, YVYU, BGRA, RGBA,
 * BGR3, BGRX, and Y800 (grayscale). If the format is unsupported, a warning is logged.
 *
 * @param frame Pointer to the input `obs_source_frame`.
 * @return A `cv::Mat` containing the frame data in BGRA format. Returns an empty
 *         matrix if the conversion fails or the format is unsupported.
 */
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
		     "[Aruco] Formato de video no soportado para conversion a BGRA: %s (%d)",
		     get_video_format_name(frame->format), frame->format);
		break;
	}

	if (bgra.empty()) {
		blog(LOG_WARNING,
		     "[Aruco] La conversion del frame a BGRA fallo para el formato %s.",
		     get_video_format_name(frame->format));
	}

	return bgra;
}

extern "C" {
/**
 * @brief Loads camera calibration parameters from a file.
 *
 * Reads a YAML or XML file containing the camera matrix and distortion coefficients,
 * which are essential for accurate 3D pose estimation. The file is expected to
 * have "camera_matrix" and "distortion_coefficients" fields.
 *
 * @param filename Path to the calibration file.
 * @param camera_matrix Output `cv::Mat` to store the camera intrinsic matrix.
 * @param dist_coeffs Output `cv::Mat` to store the distortion coefficients.
 * @return `true` if the file is opened and parameters are loaded successfully,
 *         `false` otherwise.
 */
// load_camera_calibration (sin cambios)
bool load_camera_calibration(const std::string &filename,
			     cv::Mat &camera_matrix, cv::Mat &dist_coeffs)
{
	camera_matrix.release();
	dist_coeffs.release();
	cv::FileStorage fs(filename, cv::FileStorage::READ);
	if (!fs.isOpened()) {
		blog(LOG_ERROR,
		     "[CUBE] No se pudo abrir el archivo de calibracion: %s",
		     filename.c_str());
		return false;
	}

	fs["camera_matrix"] >> camera_matrix;
	fs["distortion_coefficients"] >> dist_coeffs;

	if (camera_matrix.empty() || dist_coeffs.empty()) {
		blog(LOG_ERROR,
		     "[CUBE] Parametros invalidos en el archivo de calibracion.");
		return false;
	}

	blog(LOG_INFO, "[CUBE] Archivo de calibracion cargado con exito.");
	return true;
}

/**
 * @brief Sets the camera calibration for the ArUco detector from a file.
 *
 * This function attempts to load calibration data from the specified file. If
 * loading fails or the filename is invalid, it falls back to default camera
 * parameters by calling `set_default_camera_params`.
 *
 * @param det Pointer to the ArucoDetector structure.
 * @param filename Path to the camera calibration file.
 * @return `true` if calibration is set (either from file or default), `false`
 *         if the detector or filename is null.
 */
// set_camera_calibration (sin cambios)
bool set_camera_calibration(ArucoDetector *det, const char *filename)
{
	if (!det || !filename || *filename == '\0')
		return false;

	if (!load_camera_calibration(filename, det->camera_matrix,
				     det->dist_coeffs)) {
		blog(LOG_WARNING,
		     "[CUBE] Usando parametros de camara por defecto.");
		set_default_camera_params(det);
	} else {
		det->calibration_path = filename;
		blog(LOG_INFO,
		     "[CUBE] Calibracion cargada y establecida desde %s",
		     filename);
	}

	return true;
}

/**
 * @brief Initializes and configures a new ArucoDetector instance.
 *
 * This function allocates memory for an `ArucoDetector` structure and initializes it
 * with the specified marker size, dictionary, and calibration file. If the
 * calibration file cannot be loaded, it applies default camera parameters.
 *
 * @param marker_size_meters The physical size of the ArUco markers in meters.
 * @param dict The identifier for the ArUco dictionary to be used (e.g., ARUCO_DICT_4X4_100).
 * @param calibration_file Path to the camera calibration file.
 * @return A pointer to the newly created `ArucoDetector` instance.
 */
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
		     "[CUBE] Usando parametros de camara por defecto.");
		det->camera_matrix = (cv::Mat_<double>(3, 3) << 2000.0, 0.0,
				      1233.0, 0.0, 2000.0, 718.0, 0.0, 0.0,
				      1.0);
		det->dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);
	}
	return det;
}

/**
 * @brief Frees the resources associated with an ArucoDetector instance.
 *
 * Releases all OpenCV-related matrices and pointers (`dictionary`, `detector_params`,
 * `camera_matrix`, `dist_coeffs`, `mat_bgra`) and deallocates the `ArucoDetector`
 * structure itself.
 *
 * @param det Pointer to the ArucoDetector to be cleaned up.
 */
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

/**
 * @brief Converts a 3x3 rotation matrix to Euler angles (pitch, yaw, roll).
 *
 * This function calculates the Euler angles from a given rotation matrix, handling
 * the gimbal lock singularity case. The resulting angles are converted from radians
 * to degrees.
 *
 * @param R The input 3x3 rotation matrix (cv::Mat).
 * @param pitch Output for the pitch angle in degrees (rotation around X-axis).
 * @param yaw Output for the yaw angle in degrees (rotation around Y-axis).
 * @param roll Output for the roll angle in degrees (rotation around Z-axis).
 */
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

/**
 * @brief Computes the screen coordinates of a point projected along the marker's normal.
 *
 * This function projects a 3D point from the marker's local Z-axis into the 2D image
 * plane to determine the "up" direction for text or overlays. The coordinates are
 * scaled and clamped to fit within the final output dimensions.
 *
 * @param det Pointer to the ArucoDetector containing camera parameters.
 * @param rvec The rotation vector of the marker.
 * @param tvec The translation vector of the marker.
 * @param frame_w Width of the original video frame.
 * @param frame_h Height of the original video frame.
 * @param base_w Width of the OBS base canvas.
 * @param base_h Height of the OBS base canvas.
 * @param out_w Width of the final OBS output.
 * @param out_h Height of the final OBS output.
 * @param out_tip_x Pointer to store the resulting X coordinate.
 * @param out_tip_y Pointer to store the resulting Y coordinate.
 * @param out_valid Pointer to a boolean that will be set to true if the computation is successful.
 */
static void compute_normal_tip_screen(const ArucoDetector *det,
				      const cv::Vec3d &rvec,
				      const cv::Vec3d &tvec,
				      int frame_w, int frame_h,
				      int base_w, int base_h,
				      int out_w, int out_h,
				      float *out_tip_x,
				      float *out_tip_y,
				      bool *out_valid)
{
	if (!out_tip_x || !out_tip_y || !out_valid || !det || frame_w <= 0 ||
	    frame_h <= 0 || base_w <= 0 || base_h <= 0 || out_w <= 0 || out_h <= 0) {
		if (out_valid)
			*out_valid = false;
		return;
	}

	/* Proyectamos el eje Z local del marcador para orientar el texto "hacia arriba".
	 * Se usa -Z para tomar la dirección que sale del papel según convención de ArUco. */
	std::vector<cv::Point3f> obj_pts;
	obj_pts.emplace_back(0.0f, 0.0f, 0.0f);
	obj_pts.emplace_back(0.0f, 0.0f, -det->marker_size);

	std::vector<cv::Point2f> img_pts;
	cv::projectPoints(obj_pts, rvec, tvec, det->camera_matrix, det->dist_coeffs, img_pts);
	if (img_pts.size() < 2) {
		*out_valid = false;
		return;
	}

	const float sx_base = (float)base_w / (float)frame_w;
	const float sy_base = (float)base_h / (float)frame_h;
	const float sx_out = (float)out_w / (float)base_w;
	const float sy_out = (float)out_h / (float)base_h;

	float tip_x = img_pts[1].x * sx_base * sx_out;
	float tip_y = img_pts[1].y * sy_base * sy_out;
	tip_x = std::clamp(tip_x, 0.0f, (float)out_w);
	tip_y = std::clamp(tip_y, 0.0f, (float)out_h);

	*out_tip_x = tip_x;
	*out_tip_y = tip_y;
	*out_valid = true;
}

/**
 * @brief Processes a video frame to detect a specific ArUco marker and estimate its pose.
 *
 * This function takes an OBS video frame, converts it to a processable format (BGRA),
 * detects ArUco markers, and if the target marker (specified by `det->id`) is found,
 * it computes its 3D pose (translation and rotation), screen position, and corner
 * coordinates.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @param frame The input OBS video frame.
 * @param base_w Width of the OBS base canvas.
 * @param base_h Height of the OBS base canvas.
 * @param fw Width of the final OBS output (used for scaling).
 * @param fh Height of the final OBS output (used for scaling).
 * @param res Pointer to an ArucoResult structure to store the detection results.
 * @return `true` if the target marker was detected and processed successfully, `false` otherwise.
 */
bool process_frame_rgba(ArucoDetector *det, struct obs_source_frame *frame,
			int base_w, int base_h, int fw, int fh,
			ArucoResult *res)
{
	if (!det || !frame || !res)
		return false;
	res->normal_tip_valid = false;

	int w = base_w; // ancho base de referencia
	int h = base_h; // alto base de referencia

	// Convertir frame a BGRA
	cv::Mat bgra = obs_frame_to_bgra(frame);
	if (bgra.empty()) {
		blog(LOG_WARNING,
		     "process_frame_rgba_scaled: conversion a BGRA fallo");
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

	// Buscar marcador de interes
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
	const double scale_x = double(fw) / double(base_w);
	const double scale_y = double(fh) / double(base_h);
	for (int i = 0; i < 4; ++i) {
		float vx_base = corners[marker_index][i].x * ((float)base_w / frame->width);
		float vy_base = corners[marker_index][i].y * ((float)base_h / frame->height);

		vx_base = std::clamp(vx_base, 0.0f, float(w - 1));
		vy_base = std::clamp(vy_base, 0.0f, float(h - 1));

		const float vx_screen = std::clamp(float(vx_base * scale_x), 0.0f, float(fw));
		const float vy_screen = std::clamp(float(vy_base * scale_y), 0.0f, float(fh));

		/* Guardamos esquinas en coordenadas de pantalla OBS para alinear el overlay AR 1:1. */
		res->corners[i][0] = vx_screen;
		res->corners[i][1] = vy_screen;

		im_cx += vx_base;
		im_cy += vy_base;
	}
	im_cx /= 4.0f;
	im_cy /= 4.0f;

	// Escalar al tamano de salida OBS
	res->screen_pos_x = float(im_cx * scale_x);
	res->screen_pos_y = float(im_cy * scale_y);

	res->screen_pos_x = std::clamp(res->screen_pos_x, 0.0f, float(fw));
	res->screen_pos_y = std::clamp(res->screen_pos_y, 0.0f, float(fh));

	compute_normal_tip_screen(det, rvecs[marker_index], tvecs[marker_index],
				  frame->width, frame->height,
				  base_w, base_h, fw, fh,
				  &res->normal_tip_x, &res->normal_tip_y,
				  &res->normal_tip_valid);

	cv::Mat R;
	cv::Rodrigues(rvecs[marker_index], R);
	float pitch, yaw, roll;
	rotation_to_euler(R, pitch, yaw, roll);
	
	res->euler_x = pitch;
	res->euler_y = yaw;
	res->euler_z =-roll; // Mantenemos el -roll por si es un ajuste de ejes (OpenCV vs OBS)
	

	return true;
}

/**
 * @brief Checks if a given marker ID is present in a list of allowed IDs.
 *
 * A helper function to determine if a detected marker's ID is one of the IDs
 * the system is configured to track.
 *
 * @param id The marker ID to check.
 * @param allowed_ids An array of integers representing the allowed marker IDs.
 * @param allowed_count The number of elements in the `allowed_ids` array.
 * @return `true` if the ID is in the allowed list, `false` otherwise.
 */
static inline bool id_permitido(int id, const int *allowed_ids, size_t allowed_count)
{
	if (!allowed_ids || allowed_count == 0)
		return false;

	for (size_t i = 0; i < allowed_count; i++) {
		if (allowed_ids[i] == id)
			return true;
	}
	return false;
}

/**
 * @brief Calculates the area of a quadrilateral defined by four points.
 *
 * This function uses the Shoelace formula to compute the area of the quadrilateral
 * formed by the corners of a detected marker. This is useful for determining which
 * marker is largest on screen.
 *
 * @param c A vector of four `cv::Point2f` representing the corners of the quadrilateral.
 * @return The area of the quadrilateral as a float. Returns 0.0f if the input
 *         does not have four points.
 */
static inline float area_cuadrilatero(const std::vector<cv::Point2f> &c)
{
	/* Area por formula del "shoelace" (en coordenadas de imagen).
	 * Se usa para elegir el marcador mas grande en pantalla.
	 */
	if (c.size() < 4)
		return 0.0f;

	float a = 0.0f;
	for (int i = 0; i < 4; i++) {
		const int j = (i + 1) & 3;
		a += c[i].x * c[j].y - c[j].x * c[i].y;
	}
	return std::fabs(a) * 0.5f;
}

/**
 * @brief Processes a video frame to find the first available marker from a list of allowed IDs.
 *
 * This function is similar to `process_frame_rgba`, but instead of searching for a single
 * specific marker, it searches for any marker whose ID is in the `allowed_ids` list.
 * It selects the first one it finds in the detection results and computes its pose
 * and position.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @param frame The input OBS video frame.
 * @param base_w Width of the OBS base canvas.
 * @param base_h Height of the OBS base canvas.
 * @param fw Width of the final OBS output.
 * @param fh Height of the final OBS output.
 * @param allowed_ids An array of marker IDs to search for.
 * @param allowed_count The number of IDs in the `allowed_ids` array.
 * @param res Pointer to an ArucoResult structure to store the detection results.
 * @return `true` if a permitted marker was detected and processed, `false` otherwise.
 */
bool process_frame_rgba_select_ids(ArucoDetector *det, struct obs_source_frame *frame,
				  int base_w, int base_h, int fw, int fh,
				  const int *allowed_ids, size_t allowed_count,
				  ArucoResult *res)
{
	if (!det || !frame || !res)
		return false;
	res->normal_tip_valid = false;

	if (!allowed_ids || allowed_count == 0) {
		res->detected = false;
		return false;
	}

	int w = base_w; // ancho base de referencia
	int h = base_h; // alto base de referencia

	// Convertir frame a BGRA
	cv::Mat bgra = obs_frame_to_bgra(frame);
	if (bgra.empty()) {
		blog(LOG_WARNING,
		     "[Aruco] process_frame_rgba_select_ids: conversion a BGRA fallo");
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

	// Elegir el primer marcador permitido que aparezca en la lista de deteccion
	int marker_index = -1;
	for (int i = 0; i < (int)ids.size(); ++i) {
		if (id_permitido(ids[i], allowed_ids, allowed_count)) {
			marker_index = i;
			break;
		}
	}

	if (marker_index == -1) {
		res->detected = false;
		return false;
	}

	// Estimar pose (para todos) y seleccionar el indice elegido
	std::vector<cv::Vec3d> rvecs, tvecs;
	cv::aruco::estimatePoseSingleMarkers(corners, det->marker_size,
					     det->camera_matrix,
					     det->dist_coeffs, rvecs, tvecs);
	if (rvecs.empty() || tvecs.empty() || (int)rvecs.size() <= marker_index ||
	    (int)tvecs.size() <= marker_index) {
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
	const double scale_x = double(fw) / double(base_w);
	const double scale_y = double(fh) / double(base_h);
	for (int i = 0; i < 4; ++i) {
		float vx_base = corners[marker_index][i].x * ((float)base_w / frame->width);
		float vy_base = corners[marker_index][i].y * ((float)base_h / frame->height);

		vx_base = std::clamp(vx_base, 0.0f, float(w - 1));
		vy_base = std::clamp(vy_base, 0.0f, float(h - 1));

		const float vx_screen = std::clamp(float(vx_base * scale_x), 0.0f, float(fw));
		const float vy_screen = std::clamp(float(vy_base * scale_y), 0.0f, float(fh));

		/* Guardamos esquinas en coordenadas de pantalla OBS para alinear el overlay AR 1:1. */
		res->corners[i][0] = vx_screen;
		res->corners[i][1] = vy_screen;

		im_cx += vx_base;
		im_cy += vy_base;
	}
	im_cx /= 4.0f;
	im_cy /= 4.0f;

	// Escalar al tamano de salida OBS
	res->screen_pos_x = float(im_cx * scale_x);
	res->screen_pos_y = float(im_cy * scale_y);

	res->screen_pos_x = std::clamp(res->screen_pos_x, 0.0f, float(fw));
	res->screen_pos_y = std::clamp(res->screen_pos_y, 0.0f, float(fh));

	compute_normal_tip_screen(det, rvecs[marker_index], tvecs[marker_index],
				  frame->width, frame->height,
				  base_w, base_h, fw, fh,
				  &res->normal_tip_x, &res->normal_tip_y,
				  &res->normal_tip_valid);

	cv::Mat R;
	cv::Rodrigues(rvecs[marker_index], R);
	float pitch, yaw, roll;
	rotation_to_euler(R, pitch, yaw, roll);

	res->euler_x = pitch;
	res->euler_y = yaw;
	res->euler_z = -roll; // Mantenemos el -roll por si es un ajuste de ejes (OpenCV vs OBS)

	return true;
}

/**
 * @brief Sets the ArUco dictionary for the detector.
 *
 * Changes the active ArUco dictionary based on an integer ID. It maps this ID
 * to the corresponding predefined dictionary in OpenCV. If the ID is unknown,
 * it defaults to `DICT_4X4_100`.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @param dict_id The integer identifier for the desired dictionary (e.g., ARUCO_DICT_4X4_100).
 */
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
		// Fallback seguro si el ID no es valido
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

/**
 * @brief Sets the physical size of the markers to be detected.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @param size The marker size in meters.
 */
void set_marker_size(ArucoDetector *const det, float size)
{
	det->marker_size = size;
}

/**
 * @brief Sets the specific marker ID to be tracked.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @param id The integer ID of the marker to track.
 */
void set_marker_id(ArucoDetector *const det, int id)
{
	det->id = id;
}

/**
 * @brief Gets the currently configured marker dictionary ID.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @return The integer ID of the current marker dictionary.
 */
const int get_marker_dictionary(const ArucoDetector *const det)
{
	return det->marker_dict;
}

/**
 * @brief Gets the currently configured marker size.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @return The marker size in meters.
 */
const int get_marker_size(const ArucoDetector *const det)
{
	return det->marker_size;
}

/**
 * @brief Gets the currently configured marker ID to be tracked.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @return The integer ID of the marker being tracked.
 */
const int get_marker_id(const ArucoDetector *const det)
{
	return det->id;
}

/**
 * @brief Gets the path to the currently loaded camera calibration file.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @return A C-style string with the path to the calibration file, or NULL if not set.
 */
const char *get_calibration_path(const ArucoDetector *const det)
{
	return det ? det->calibration_path.c_str() : NULL;
}

/**
 * @brief Sets the camera calibration by loading a new file.
 *
 * If the file is loaded successfully, the detector's calibration path is updated.
 * If it fails, the detector's parameters are reset to default values.
 *
 * @param det Pointer to the ArucoDetector instance.
 * @param path The path to the new camera calibration file.
 */
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
