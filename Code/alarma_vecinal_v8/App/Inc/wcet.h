/*
 * app_wcet.h — medición de WCET por FSM usando DWT cycle counter
 *
 * Uso en app.c (comentar/descomentar para activar medición):
 *
 *   wcet_start(); sensor_btn_panic_update(); wcet_stop(WCET_SENSOR_BTN_PANIC);
 *   wcet_start(); system_update();           wcet_stop(WCET_SYSTEM);
 *   wcet_start(); act_led_blue_update();     wcet_stop(WCET_ACT_LED_BLUE);
 *
 * Resultado: inspeccionar wcet_max[] desde el debugger (Live Expressions).
 * Unidad: microsegundos.
 *
 * Para calcular factor U:
 *   U = Σ (wcet_max[i] / 1000)    (Ti = 1 ms = 1000 µs para todas las tareas)
 */

#ifndef WCET_H
#define WCET_H

#include <stdint.h>

typedef enum
{
    WCET_SENSOR_BTN_PANIC,
    WCET_SENSOR_LDR,
    WCET_GSM,
    WCET_BLE,
    WCET_EEPROM,
    WCET_SYSTEM,
    WCET_ACT_LED_BLUE,
    WCET_ACT_LED_WHITE,
    WCET_ACT_LED_ALARM,
    WCET_ACT_LED_NETWORK,
    WCET_COUNT /* tamaño del array . Agregar entradas antes de esta línea */
} Wcet_Id;

extern uint32_t wcet_max[WCET_COUNT]; /* inspeccionar desde el debugger */

void wcet_init(void);       /* llamar una vez en app_init()  */
void wcet_start(void);      /* llamar antes de la FSM        */
void wcet_stop(Wcet_Id id); /* llamar después de la FSM      */

#endif /* APP_WCET_H */
