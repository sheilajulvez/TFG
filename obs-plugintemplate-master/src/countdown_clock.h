/**
 * @file countdown_clock.h
 * @brief Módulo de reloj de cuenta atrás (countdown timer).
 *
 * Gestiona el estado interno del temporizador: duración, tiempo restante
 * y ángulos de las manecillas para un reloj analógico que muestra
 * el tiempo restante. Diseñado para ser modular y expandible.
 *
 * Las manecillas indican tiempo restante: por ejemplo, si quedan 6 horas,
 * la manecilla de horas apunta a las 6 (en un dial de 12h).
 */

#ifndef COUNTDOWN_CLOCK_H
#define COUNTDOWN_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Estado del countdown */
typedef enum countdown_state {
	COUNTDOWN_STATE_STOPPED = 0,
	COUNTDOWN_STATE_RUNNING,
	COUNTDOWN_STATE_PAUSED,
	COUNTDOWN_STATE_FINISHED
} countdown_state_t;

/** Estructura opaca del reloj (implementación en .c) */
typedef struct countdown_clock countdown_clock_t;

/**
 * Crea un reloj de cuenta atrás.
 * @return Instancia nueva o NULL si falla.
 */
countdown_clock_t *countdown_clock_create(void);

/**
 * Destruye el reloj y libera recursos.
 */
void countdown_clock_destroy(countdown_clock_t *clock);

/**
 * Establece la duración total del countdown (en segundos).
 * No reinicia el reloj; usar countdown_clock_start() después.
 */
void countdown_clock_set_duration(countdown_clock_t *clock, uint32_t total_seconds);

/**
 * Establece la duración en horas, minutos y segundos.
 */
void countdown_clock_set_duration_hms(countdown_clock_t *clock,
                                      uint32_t hours, uint32_t minutes, uint32_t seconds);

/**
 * Inicia la cuenta atrás desde la duración configurada.
 */
void countdown_clock_start(countdown_clock_t *clock);

/**
 * Pausa el countdown (el tiempo restante se mantiene).
 */
void countdown_clock_pause(countdown_clock_t *clock);

/**
 * Reanuda si estaba pausado.
 */
void countdown_clock_resume(countdown_clock_t *clock);

/**
 * Reinicia al estado inicial (duración actual) sin iniciar.
 */
void countdown_clock_reset(countdown_clock_t *clock);

/**
 * Actualiza el estado del reloj (llamar desde video_tick con delta en segundos).
 * No debe bloquear; solo actualiza tiempo restante y ángulos.
 */
void countdown_clock_tick(countdown_clock_t *clock, float delta_seconds);

/**
 * Sincroniza el tiempo restante desde una fuente externa (p. ej. API web).
 * Ajusta el countdown para que muestre exactamente h, m, s restantes.
 */
void countdown_clock_sync_remaining(countdown_clock_t *clock,
                                    uint32_t hours, uint32_t minutes, uint32_t seconds);

/**
 * Obtiene el estado actual.
 */
countdown_state_t countdown_clock_get_state(const countdown_clock_t *clock);

/**
 * Obtiene el tiempo restante (horas, minutos, segundos enteros).
 */
void countdown_clock_get_remaining(const countdown_clock_t *clock,
                                  uint32_t *out_hours, uint32_t *out_minutes, uint32_t *out_seconds);

/**
 * Obtiene el tiempo restante total en segundos (puede incluir fracción si se desea en el futuro).
 */
double countdown_clock_get_remaining_seconds(const countdown_clock_t *clock);

/**
 * Ángulos en grados (0–360) para las manecillas del reloj analógico.
 * Convención: 0° = "12 en punto", sentido horario positivo (o antihorario según modelo 3D).
 * - hour_deg: manecilla de horas (suele usar dial de 12h).
 * - minute_deg: manecilla de minutos (60 divisiones).
 * - second_deg: manecilla de segundos (60 divisiones).
 */
void countdown_clock_get_hand_angles(const countdown_clock_t *clock,
                                    float *hour_deg, float *minute_deg, float *second_deg);

/**
 * Establece el número máximo de horas en el dial (12 o 24) para el cálculo de la manecilla de horas.
 * Por defecto 12.
 */
void countdown_clock_set_dial_hours(countdown_clock_t *clock, uint32_t max_hours);

/**
 * Obtiene el ángulo para una manecilla única que representa el tiempo restante total.
 * El ángulo se calcula como: (tiempo_restante / duración_total) * 360 grados.
 * Si la duración es 0, devuelve 0 grados.
 * @param clock Instancia del reloj
 * @param single_hand_deg Puntero donde se almacenará el ángulo en grados (0-360)
 */
void countdown_clock_get_single_hand_angle(const countdown_clock_t *clock,
                                           float *single_hand_deg);


#ifdef __cplusplus
}
#endif

#endif /* COUNTDOWN_CLOCK_H */
