/**
 * @file countdown_clock.c
 * @brief Implementación del reloj de cuenta atrás.
 *
 *  estado (stopped/running/paused/finished), duración,
 * tiempo restante y ángulos de manecillas.
 */

#include "countdown_clock.h"
#include <stdlib.h>
#include <string.h>
#include <obs-module.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct countdown_clock {
	
	uint32_t duration_seconds;
	double remaining_seconds;
	countdown_state_t state;
	uint32_t dial_max_hours;
};

countdown_clock_t *countdown_clock_create()
{
	countdown_clock_t *clock = (countdown_clock_t *)malloc(sizeof(struct countdown_clock));
	if (!clock)return NULL;
	memset(clock, 0, sizeof(struct countdown_clock));
	clock->state = COUNTDOWN_STATE_STOPPED;
	clock->dial_max_hours = 12;
	return clock;
}

void countdown_clock_destroy(countdown_clock_t *clock)
{
	if (clock)free(clock);
}

void countdown_clock_set_duration(countdown_clock_t *clock, uint32_t total_seconds)
{
	if (!clock)
	return;
	clock->duration_seconds = total_seconds;
	clock->remaining_seconds = (double)total_seconds;
}

void countdown_clock_set_duration_hms(countdown_clock_t *clock,
                                      uint32_t hours, uint32_t minutes, 
									  uint32_t seconds)
{
	if (!clock)return;
	clock->duration_seconds = hours * 3600u + minutes * 60u + seconds;
	clock->remaining_seconds = (double)clock->duration_seconds;
}

void countdown_clock_start(countdown_clock_t *clock)
{
	if (!clock)
		return;
	clock->remaining_seconds = (double)clock->duration_seconds;
	clock->state = COUNTDOWN_STATE_RUNNING;
}

void countdown_clock_pause(countdown_clock_t *clock)
{
	if (!clock)
		return;
	if (clock->state == COUNTDOWN_STATE_RUNNING)
		clock->state = COUNTDOWN_STATE_PAUSED;
}

void countdown_clock_resume(countdown_clock_t *clock)
{
	if (!clock)
		return;
	if (clock->state == COUNTDOWN_STATE_PAUSED)
		clock->state = COUNTDOWN_STATE_RUNNING;
}

void countdown_clock_reset(countdown_clock_t *clock)
{
	if (!clock)
		return;
	clock->remaining_seconds = (double)clock->duration_seconds;
	clock->state = COUNTDOWN_STATE_STOPPED;
}

void countdown_clock_tick(countdown_clock_t *clock, float delta_seconds)
{
	if (!clock || clock->state != COUNTDOWN_STATE_RUNNING)
		return;
	clock->remaining_seconds -= (double)delta_seconds;
	if (clock->remaining_seconds <= 0.0) {
		clock->remaining_seconds = 0.0;
		clock->state = COUNTDOWN_STATE_FINISHED;
	}
}

void countdown_clock_sync_remaining(countdown_clock_t *clock,
				     uint32_t hours, uint32_t minutes,
				     uint32_t seconds)
{
	if (!clock)
		return;
	clock->remaining_seconds = (double)(hours * 3600 + minutes * 60 + seconds);
}

void countdown_clock_sync_full(countdown_clock_t *clock, double remaining,
			       double total_duration)
{
	if (!clock)
		return;
	clock->remaining_seconds = remaining;
	if (total_duration > 0)
		clock->duration_seconds = (uint32_t)(total_duration + 0.5);
	/* Al sincronizar desde web, el reloj debe seguir avanzando entre consultas. */
	if (clock->remaining_seconds > 0.0) {
		clock->state = COUNTDOWN_STATE_RUNNING;
	} else {
		clock->remaining_seconds = 0.0;
		clock->state = COUNTDOWN_STATE_FINISHED;
	}

	blog(LOG_INFO,
	     "[COUNTDOWN] Sincronizado desde web. Restante=%.2fs Total=%.2fs Estado=%d",
	     clock->remaining_seconds, (double)clock->duration_seconds,
	     (int)clock->state);
}

countdown_state_t countdown_clock_get_state(const countdown_clock_t *clock)
{
	return clock ? clock->state : COUNTDOWN_STATE_STOPPED;
}

void countdown_clock_get_remaining(const countdown_clock_t *clock,
                                  uint32_t *out_hours, uint32_t *out_minutes, uint32_t *out_seconds)
{
	if (!clock) {
		if (out_hours) *out_hours = 0;
		if (out_minutes) *out_minutes = 0;
		if (out_seconds) *out_seconds = 0;
		return;
	}
	uint32_t total = (uint32_t)(clock->remaining_seconds + 0.5);
	if (total > 0xFFFFFFFF / 3600)
		total = 0;
	if (out_hours) *out_hours = total / 3600;
	if (out_minutes) *out_minutes = (total % 3600) / 60;
	if (out_seconds) *out_seconds = total % 60;
}

double countdown_clock_get_remaining_seconds(const countdown_clock_t *clock)
{
	if (!clock)
		return 0;
	return  clock->remaining_seconds;
}
//te calcula en agnulo de los segundos, minutos y horas, teniendo en cuenta el maximo de horas del relojito
#include <math.h>

void countdown_clock_get_hand_angles(const countdown_clock_t *clock,
				     float *hour_deg, float *minute_deg,
				     float *second_deg)
{
	if (!clock) {
		if (hour_deg)
			*hour_deg = 0.0f;
		if (minute_deg)
			*minute_deg = 0.0f;
		if (second_deg)
			*second_deg = 0.0f;
		return;
	}

	float rem = (float)clock->remaining_seconds;
	if (rem < 0.0f)
		rem = 0.0f;

	if (second_deg) {
		*second_deg = fmodf(rem, 60.0f) * 6.0f;
	}

	if (minute_deg) {
		float total_minutes = floorf(rem / 60.0f);
		*minute_deg = fmodf(total_minutes, 60.0f) * 6.0f;
	}

	if (hour_deg) {
		float max_h = clock->dial_max_hours? (float)clock->dial_max_hours: 24.0f;
		float total_hours = floorf(rem / 3600.0f);
		*hour_deg = fmodf(total_hours, max_h) * (360.0f / max_h);
	}
}

void countdown_clock_set_dial_hours(countdown_clock_t *clock, uint32_t max_hours)
{
	if (!clock)
		return;
	clock->dial_max_hours = (max_hours == 0) ? 24 : max_hours;
}
//devuelve el angulo de una manecilla unica que representa el tiempo restante total,
// calculado como (tiempo_restante / duración_total) * 360 grados
void countdown_clock_get_single_hand_angle(const countdown_clock_t *clock,
					  float *single_hand_deg)
{
	if (!single_hand_deg)
		return;

	if (!clock || clock->duration_seconds == 0) {
		*single_hand_deg = 0.0f;
		return;
	}

	float rem = (float)clock->remaining_seconds;
	if (rem < 0.0f)
		rem = 0.0f;

	float dur = (float)clock->duration_seconds;
	float angle = (rem / dur) * 360.0f;

	*single_hand_deg = fmodf(angle, 360.0f);
}
