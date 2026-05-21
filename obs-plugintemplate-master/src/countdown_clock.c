#include "countdown_clock.h"

#include <math.h>
#include <obs-module.h>
#include <stdlib.h>
#include <string.h>

struct countdown_clock {
	uint32_t duration_seconds;
	double remaining_seconds;
	countdown_state_t state;
	uint32_t dial_max_hours;
};

// Creates a new countdown clock.
// Crea un nuevo reloj de cuenta atras.
countdown_clock_t *countdown_clock_create(void)
{
	countdown_clock_t *clock =
		(countdown_clock_t *)malloc(sizeof(struct countdown_clock));
	if (!clock)
		return NULL;

	memset(clock, 0, sizeof(struct countdown_clock));
	clock->state = COUNTDOWN_STATE_STOPPED;
	clock->dial_max_hours = 12;
	return clock;
}

// Releases the countdown clock resources.
// Libera los recursos del reloj de cuenta atras.
void countdown_clock_destroy(countdown_clock_t *clock)
{
	if (clock)
		free(clock);
}

// Sets the total countdown duration in seconds.
// Establece la duracion total de la cuenta atras en segundos.
void countdown_clock_set_duration(countdown_clock_t *clock,
				  uint32_t total_seconds)
{
	if (!clock)
		return;

	clock->duration_seconds = total_seconds;
	clock->remaining_seconds = (double)total_seconds;
}

// Sets the countdown duration in hours, minutes, and seconds.
// Establece la duracion de la cuenta atras en horas, minutos y segundos.
void countdown_clock_set_duration_hms(countdown_clock_t *clock,
				      uint32_t hours, uint32_t minutes,
				      uint32_t seconds)
{
	if (!clock)
		return;

	clock->duration_seconds = hours * 3600u + minutes * 60u + seconds;
	clock->remaining_seconds = (double)clock->duration_seconds;
}

// Starts the countdown from the configured duration.
// Inicia la cuenta atras desde la duracion configurada.
void countdown_clock_start(countdown_clock_t *clock)
{
	if (!clock)
		return;

	clock->remaining_seconds = (double)clock->duration_seconds;
	clock->state = COUNTDOWN_STATE_RUNNING;
}

// Pauses the countdown.
// Pausa la cuenta atras.
void countdown_clock_pause(countdown_clock_t *clock)
{
	if (!clock)
		return;

	if (clock->state == COUNTDOWN_STATE_RUNNING)
		clock->state = COUNTDOWN_STATE_PAUSED;
}

// Resumes the countdown if it is paused.
// Reanuda la cuenta atras si esta pausada.
void countdown_clock_resume(countdown_clock_t *clock)
{
	if (!clock)
		return;

	if (clock->state == COUNTDOWN_STATE_PAUSED)
		clock->state = COUNTDOWN_STATE_RUNNING;
}

// Resets the countdown without starting it.
// Reinicia la cuenta atras sin iniciarla.
void countdown_clock_reset(countdown_clock_t *clock)
{
	if (!clock)
		return;

	clock->remaining_seconds = (double)clock->duration_seconds;
	clock->state = COUNTDOWN_STATE_STOPPED;
}

// Advances the countdown by the given delta time.
// Avanza la cuenta atras con el delta de tiempo indicado.
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

// Synchronizes the remaining countdown time.
// Sincroniza el tiempo restante de la cuenta atras.
void countdown_clock_sync_remaining(countdown_clock_t *clock,
				    uint32_t hours, uint32_t minutes,
				    uint32_t seconds)
{
	if (!clock)
		return;

	clock->remaining_seconds =
		(double)(hours * 3600u + minutes * 60u + seconds);
}

// Synchronizes the remaining and total countdown time.
// Sincroniza el tiempo restante y total de la cuenta atras.
void countdown_clock_sync_full(countdown_clock_t *clock, double remaining,
			       double total_duration)
{
	if (!clock)
		return;

	clock->remaining_seconds = remaining;
	if (total_duration > 0.0)
		clock->duration_seconds = (uint32_t)(total_duration + 0.5);

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

// Returns the current countdown state.
// Devuelve el estado actual de la cuenta atras.
countdown_state_t countdown_clock_get_state(const countdown_clock_t *clock)
{
	return clock ? clock->state : COUNTDOWN_STATE_STOPPED;
}

// Returns the remaining time as hours, minutes, and seconds.
// Devuelve el tiempo restante en horas, minutos y segundos.
void countdown_clock_get_remaining(const countdown_clock_t *clock,
				   uint32_t *out_hours,
				   uint32_t *out_minutes,
				   uint32_t *out_seconds)
{
	if (!clock) {
		if (out_hours)
			*out_hours = 0;
		if (out_minutes)
			*out_minutes = 0;
		if (out_seconds)
			*out_seconds = 0;
		return;
	}

	uint32_t total = (uint32_t)(clock->remaining_seconds + 0.5);
	if (total > 0xFFFFFFFF / 3600)
		total = 0;

	if (out_hours)
		*out_hours = total / 3600;
	if (out_minutes)
		*out_minutes = (total % 3600) / 60;
	if (out_seconds)
		*out_seconds = total % 60;
}

// Returns the remaining time in seconds.
// Devuelve el tiempo restante en segundos.
double countdown_clock_get_remaining_seconds(const countdown_clock_t *clock)
{
	if (!clock)
		return 0.0;

	return clock->remaining_seconds;
}

// Returns the analog clock hand angles.
// Devuelve los angulos de las manecillas del reloj analogico.
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

	if (second_deg)
		*second_deg = fmodf(rem, 60.0f) * 6.0f;

	if (minute_deg) {
		float total_minutes = floorf(rem / 60.0f);
		*minute_deg = fmodf(total_minutes, 60.0f) * 6.0f;
	}

	if (hour_deg) {
		float max_h = clock->dial_max_hours ? (float)clock->dial_max_hours
						    : 24.0f;
		float total_hours = floorf(rem / 3600.0f);
		*hour_deg = fmodf(total_hours, max_h) * (360.0f / max_h);
	}
}

// Sets the maximum number of hours in the dial.
// Establece el numero maximo de horas del dial.
void countdown_clock_set_dial_hours(countdown_clock_t *clock,
				    uint32_t max_hours)
{
	if (!clock)
		return;

	clock->dial_max_hours = (max_hours == 0) ? 24 : max_hours;
}

// Returns the angle for the single countdown hand.
// Devuelve el angulo de la manecilla unica de la cuenta atras.
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
