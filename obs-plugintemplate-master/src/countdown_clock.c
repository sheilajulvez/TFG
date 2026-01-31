/**
 * @file countdown_clock.c
 * @brief Implementación del reloj de cuenta atrás.
 *
 * Lógica interna: estado (stopped/running/paused/finished), duración,
 * tiempo restante y ángulos de manecillas. Las manecillas representan
 * el tiempo restante (countdown), no la hora del día.
 */

#include "countdown_clock.h"
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct countdown_clock {
	/** Duración total del countdown en segundos */
	uint32_t duration_seconds;
	/** Tiempo restante en segundos (puede tener fracción internamente para suavizado) */
	double remaining_seconds;
	/** Estado actual */
	countdown_state_t state;
	/** Máximo de horas en el dial (12 o 24) para la manecilla de horas */
	uint32_t dial_max_hours;
};

countdown_clock_t *countdown_clock_create(void)
{
	countdown_clock_t *clock = (countdown_clock_t *)malloc(sizeof(struct countdown_clock));
	if (!clock)
		return NULL;
	memset(clock, 0, sizeof(struct countdown_clock));
	clock->state = COUNTDOWN_STATE_STOPPED;
	clock->dial_max_hours = 12;
	return clock;
}

void countdown_clock_destroy(countdown_clock_t *clock)
{
	if (clock)
		free(clock);
}

void countdown_clock_set_duration(countdown_clock_t *clock, uint32_t total_seconds)
{
	if (!clock)
		return;
	clock->duration_seconds = total_seconds;
	clock->remaining_seconds = (double)total_seconds;
}

void countdown_clock_set_duration_hms(countdown_clock_t *clock,
                                      uint32_t hours, uint32_t minutes, uint32_t seconds)
{
	if (!clock)
		return;
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
                                   uint32_t hours, uint32_t minutes, uint32_t seconds)
{
	if (!clock)
		return;
	clock->remaining_seconds = (double)(hours * 3600u + minutes * 60u + seconds);
	/* No cambiamos el estado; el tick seguirá restando si está running */
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
	if (total > 0xFFFFFFFFu / 3600u)
		total = 0;
	if (out_hours) *out_hours = total / 3600u;
	if (out_minutes) *out_minutes = (total % 3600u) / 60u;
	if (out_seconds) *out_seconds = total % 60u;
}

double countdown_clock_get_remaining_seconds(const countdown_clock_t *clock)
{
	return clock ? clock->remaining_seconds : 0.0;
}

void countdown_clock_get_hand_angles(const countdown_clock_t *clock,
				     float *hour_deg, float *minute_deg,
				     float *second_deg)
{
	if (!clock) {
		if (hour_deg)
			*hour_deg = 0.f;
		if (minute_deg)
			*minute_deg = 0.f;
		if (second_deg)
			*second_deg = 0.f;
		return;
	}

	double rem = clock->remaining_seconds;
	if (rem < 0.0)
		rem = 0.0;

	uint32_t max_h = clock->dial_max_hours ? clock->dial_max_hours : 12u;

	// descomponer en h/m/s
	uint32_t hours = (uint32_t)(rem / 3600.0);
	uint32_t minutes = (uint32_t)((rem - hours * 3600u) / 60u);
	uint32_t seconds = (uint32_t)(rem - hours * 3600u - minutes * 60u);

	double seconds_frac =
		(rem - (double)((uint32_t)(rem / 60.0) *
				60u)); // fracción de segundo (0..59.x)
	// mejor: usar rem mod 60 con la fracción:
	double sec_in_min = fmod(rem, 60.0);

	// Hour angle: incluir minutos+segundos como fracción de hora
	double hour_val = (double)(hours % max_h) +
			  ((double)minutes + sec_in_min / 60.0) / 60.0;
	double hour_angle = 360.0 * (1.0 - (hour_val / (double)max_h));

	// Minute angle: minutos dentro de la hora + fracción por segundos
	double minute_val = (double)minutes + sec_in_min / 60.0; // 0..60
	double minute_angle = 360.0 * (1.0 - (minute_val / 60.0));

	// Second angle: segundos con fracción
	double second_angle = 360.0 * (1.0 - (sec_in_min / 60.0));

	// normalizar
	while (hour_angle < 0.0)
		hour_angle += 360.0;
	while (hour_angle >= 360.0)
		hour_angle -= 360.0;
	while (minute_angle < 0.0)
		minute_angle += 360.0;
	while (minute_angle >= 360.0)
		minute_angle -= 360.0;
	while (second_angle < 0.0)
		second_angle += 360.0;
	while (second_angle >= 360.0)
		second_angle -= 360.0;

	if (hour_deg)
		*hour_deg = (float)hour_angle;
	if (minute_deg)
		*minute_deg = (float)minute_angle;
	if (second_deg)
		*second_deg = (float)second_angle;
}

void countdown_clock_set_dial_hours(countdown_clock_t *clock, uint32_t max_hours)
{
	if (!clock)
		return;
	clock->dial_max_hours = (max_hours == 0) ? 12 : max_hours;
}
