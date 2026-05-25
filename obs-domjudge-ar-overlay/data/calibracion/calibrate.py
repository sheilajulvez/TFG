#!/usr/bin/env python3
import sys
import os
import glob
import cv2
import numpy as np

def calibrate_camera(calib_dir, square_size, output_file,
                     pattern_size=(7, 6)):
    # Preparar puntos objeto (0,0,0), (1,0,0), ... en la unidad square_size
    objp = np.zeros((pattern_size[1]*pattern_size[0], 3), np.float32)
    objp[:, :2] = np.mgrid[0:pattern_size[0],
                           0:pattern_size[1]].T.reshape(-1, 2)
    objp *= square_size

    objpoints = []  # 3D en espacio real
    imgpoints = []  # 2D en imagen

    images = glob.glob(os.path.join(calib_dir, '*.jpg')) + \
             glob.glob(os.path.join(calib_dir, '*.png'))

    if not images:
        print(f"⚠️  No se encontraron imágenes en {calib_dir}")
        return False

    for fname in images:
        img = cv2.imread(fname)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        ret, corners = cv2.findChessboardCorners(gray, pattern_size,
                                                 None)
        if not ret:
            print(f"  ⚠️  No se detectó el tablero en {os.path.basename(fname)}")
            continue
        # Refina sub-píxel
        corners2 = cv2.cornerSubPix(gray, corners,
                                    winSize=(11,11),
                                    zeroZone=(-1,-1),
                                    criteria=(cv2.TERM_CRITERIA_EPS +
                                              cv2.TERM_CRITERIA_MAX_ITER,
                                              30, 0.001))
        objpoints.append(objp)
        imgpoints.append(corners2)
        print(f"  ✔️  Esquinas detectadas en {os.path.basename(fname)}")

    if len(objpoints) < 3:
        print("❌  Necesitas al menos 3 imágenes con esquinas detectadas")
        return False

    # Calibración
    ret, camera_matrix, dist_coeffs, rvecs, tvecs = \
        cv2.calibrateCamera(objpoints, imgpoints,
                            gray.shape[::-1], None, None)

    # Cálculo de error de reproyección
    total_error = 0
    for i in range(len(objpoints)):
        imgpoints2, _ = cv2.projectPoints(objpoints[i],
                                          rvecs[i], tvecs[i],
                                          camera_matrix, dist_coeffs)
        error = cv2.norm(imgpoints[i], imgpoints2,
                         cv2.NORM_L2)/len(imgpoints2)
        total_error += error
    mean_error = total_error / len(objpoints)
    print(f"Reprojection error: {mean_error}")

    # Guardar en YAML con OpenCV
    fs = cv2.FileStorage(output_file, cv2.FILE_STORAGE_WRITE)
    fs.write("camera_matrix", camera_matrix)
    fs.write("distortion_coefficients", dist_coeffs)
    fs.release()
    print(f"Parámetros de calibración guardados en {output_file}")

    return True

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Uso: python calibrate.py <calib_dir> <square_size> <output_file>")
        sys.exit(1)

    folder = sys.argv[1]
    try:
        sq = float(sys.argv[2])
    except ValueError:
        print("El <square_size> debe ser un número (p. ej. 0.025)")
        sys.exit(1)
    out = sys.argv[3]

    ok = calibrate_camera(folder, square_size=sq, output_file=out)
    sys.exit(0 if ok else 1)
