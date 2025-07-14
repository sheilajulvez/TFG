#ifndef YUV2BGRA_H
#define YUV2BGRA_H

#include <stdint.h>
#include <obs.h>


#ifdef __cplusplus
extern "C" {
#endif

// Función tipo para obtener U y V en la posición x,y
typedef void (*get_uv_func)(const struct obs_source_frame *frame, int x, int y,
			    uint8_t *u, uint8_t *v);

// Funciones para cada formato
void get_uv_i420(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v);
void get_uv_nv12(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v);
void get_uv_i422(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v);

// Función genérica para convertir YUV a BGRA
void convert_yuv_to_bgra_generic(const struct obs_source_frame *frame,
				 uint8_t *dst_bgra, get_uv_func get_uv);

#ifdef __cplusplus
}
#endif

#endif // YUV2BGRA_H
