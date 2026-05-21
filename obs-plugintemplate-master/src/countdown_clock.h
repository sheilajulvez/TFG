#ifndef COUNTDOWN_CLOCK_H
#define COUNTDOWN_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum countdown_state {
	COUNTDOWN_STATE_STOPPED = 0,
	COUNTDOWN_STATE_RUNNING,
	COUNTDOWN_STATE_PAUSED,
	COUNTDOWN_STATE_FINISHED
} countdown_state_t;

typedef struct countdown_clock countdown_clock_t;

// Creates a new countdown clock.
// Crea un nuevo reloj de cuenta atras.
countdown_clock_t *countdown_clock_create(void);

// Releases the countdown clock resources.
// Libera los recursos del reloj de cuenta atras.
void countdown_clock_destroy(countdown_clock_t *clock);

// Sets the total countdown duration in seconds.
// Establece la duracion total de la cuenta atras en segundos.
void countdown_clock_set_duration(countdown_clock_t *clock,
				  uint32_t total_seconds);

// Sets the countdown duration in hours, minutes, and seconds.
// Establece la duracion de la cuenta atras en horas, minutos y segundos.
void countdown_clock_set_duration_hms(countdown_clock_t *clock,
				      uint32_t hours, uint32_t minutes,
				      uint32_t seconds);

// Starts the countdown from the configured duration.
// Inicia la cuenta atras desde la duracion configurada.
void countdown_clock_start(countdown_clock_t *clock);

// Pauses the countdown.
// Pausa la cuenta atras.
void countdown_clock_pause(countdown_clock_t *clock);

// Resumes the countdown if it is paused.
// Reanuda la cuenta atras si esta pausada.
void countdown_clock_resume(countdown_clock_t *clock);

// Resets the countdown without starting it.
// Reinicia la cuenta atras sin iniciarla.
void countdown_clock_reset(countdown_clock_t *clock);

// Advances the countdown by the given delta time.
// Avanza la cuenta atras con el delta de tiempo indicado.
void countdown_clock_tick(countdown_clock_t *clock, float delta_seconds);

// Synchronizes the remaining countdown time.
// Sincroniza el tiempo restante de la cuenta atras.
void countdown_clock_sync_remaining(countdown_clock_t *clock,
				    uint32_t hours, uint32_t minutes,
				    uint32_t seconds);

// Synchronizes the remaining and total countdown time.
// Sincroniza el tiempo restante y total de la cuenta atras.
void countdown_clock_sync_full(countdown_clock_t *clock, double remaining,
			       double total_duration);

// Returns the current countdown state.
// Devuelve el estado actual de la cuenta atras.
countdown_state_t countdown_clock_get_state(const countdown_clock_t *clock);

// Returns the remaining time as hours, minutes, and seconds.
// Devuelve el tiempo restante en horas, minutos y segundos.
void countdown_clock_get_remaining(const countdown_clock_t *clock,
				   uint32_t *out_hours,
				   uint32_t *out_minutes,
				   uint32_t *out_seconds);

// Returns the remaining time in seconds.
// Devuelve el tiempo restante en segundos.
double countdown_clock_get_remaining_seconds(const countdown_clock_t *clock);

// Returns the analog clock hand angles.
// Devuelve los angulos de las manecillas del reloj analogico.
void countdown_clock_get_hand_angles(const countdown_clock_t *clock,
				     float *hour_deg, float *minute_deg,
				     float *second_deg);

// Sets the maximum number of hours in the dial.
// Establece el numero maximo de horas del dial.
void countdown_clock_set_dial_hours(countdown_clock_t *clock,
				    uint32_t max_hours);

// Returns the angle for the single countdown hand.
// Devuelve el angulo de la manecilla unica de la cuenta atras.
void countdown_clock_get_single_hand_angle(const countdown_clock_t *clock,
					   float *single_hand_deg);

#ifdef __cplusplus
}
#endif

#endif
