#include "yuv2bgra.h"

// Función interna para convertir un píxel YUV a BGRA
static void yuv_to_bgra(uint8_t y, uint8_t u, uint8_t v, uint8_t *b, uint8_t *g,
			uint8_t *r)
{
	int c = y - 16;
	int d = u - 128;
	int e = v - 128;

	int r_ = (298 * c + 409 * e + 128) >> 8;
	int g_ = (298 * c - 100 * d - 208 * e + 128) >> 8;
	int b_ = (298 * c + 516 * d + 128) >> 8;

	*r = (uint8_t)(r_ < 0 ? 0 : r_ > 255 ? 255 : r_);
	*g = (uint8_t)(g_ < 0 ? 0 : g_ > 255 ? 255 : g_);
	*b = (uint8_t)(b_ < 0 ? 0 : b_ > 255 ? 255 : b_);
}

// Implementaciones específicas para obtener U y V según formato YUV

void get_uv_i420(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v)
{
	int u_pitch = frame->linesize[1];
	int v_pitch = frame->linesize[2];
	const uint8_t *U = frame->data[1];
	const uint8_t *V = frame->data[2];
	*u = U[(y / 2) * u_pitch + (x / 2)];
	*v = V[(y / 2) * v_pitch + (x / 2)];
}

void get_uv_nv12(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v)
{
	int uv_pitch = frame->linesize[1];
	const uint8_t *UV = frame->data[1];
	*u = UV[(y / 2) * uv_pitch + (x & ~1)];
	*v = UV[(y / 2) * uv_pitch + (x | 1)];
}

void get_uv_i422(const struct obs_source_frame *frame, int x, int y, uint8_t *u,
		 uint8_t *v)
{
	int u_pitch = frame->linesize[1];
	int v_pitch = frame->linesize[2];
	const uint8_t *U = frame->data[1];
	const uint8_t *V = frame->data[2];
	*u = U[y * u_pitch + x];
	*v = V[y * v_pitch + x];
}

// Función genérica para convertir cualquier YUV a BGRA usando función get_uv
void convert_yuv_to_bgra_generic(const struct obs_source_frame *frame,
				 uint8_t *dst_bgra, get_uv_func get_uv)
{
	int width = frame->width;
	int height = frame->height;
	const uint8_t *Y = frame->data[0];
	int y_pitch = frame->linesize[0];

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint8_t y_val = Y[y * y_pitch + x];
			uint8_t u_val, v_val;
			get_uv(frame, x, y, &u_val, &v_val);

			uint8_t r, g, b;
			yuv_to_bgra(y_val, u_val, v_val, &b, &g, &r);

			int dst_index = (y * width + x) * 4;
			dst_bgra[dst_index + 0] = b;
			dst_bgra[dst_index + 1] = g;
			dst_bgra[dst_index + 2] = r;
			dst_bgra[dst_index + 3] = 255; // Alpha
		}
	}
}
