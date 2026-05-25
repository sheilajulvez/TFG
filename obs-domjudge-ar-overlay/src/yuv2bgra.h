#ifndef YUV2BGRA_H
#define YUV2BGRA_H

#include <stdint.h>
#include <obs.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef void (*get_uv_func)(const struct obs_source_frame *frame, int x, int y,
			    uint8_t *u, uint8_t *v);

// Gets the chroma values for the I420 format.
// Obtiene los valores de croma para el formato I420.
void get_uv_i420(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v);

// Gets the chroma values for the NV12 format.
// Obtiene los valores de croma para el formato NV12.
void get_uv_nv12(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v);

// Gets the chroma values for the I422 format.
// Obtiene los valores de croma para el formato I422.
void get_uv_i422(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v);

// Converts a YUY2 frame to BGRA.
// Convierte un frame YUY2 a BGRA.
void convert_yuy2_to_bgra(const struct obs_source_frame *frame,
			  uint8_t *dst_bgra);

// Converts a planar YUV frame to BGRA with a UV accessor.
// Convierte un frame YUV planar a BGRA con un acceso UV.
void convert_yuv_to_bgra_generic(const struct obs_source_frame *frame,
				 uint8_t *dst_bgra, get_uv_func get_uv);

#ifdef __cplusplus
}
#endif

#endif
