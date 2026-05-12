/*
 * Bus de comandos del sistema hacia los actuadores (LEDs)
 * El sistema publica comandos con put_ev_act_*().
   (ver task_system.c)
 * Cada actuador lee y consume su propio flag en su update().
   (ver task_act_led_*.c)
 * Flags independientes por LED y dirección: todos los comandos de un tick se ejecutan.
 * No bloqueante: el sistema publica y se olvida.
 */

#ifndef TASK_ACTUATOR_INTERFACE_H
#define TASK_ACTUATOR_INTERFACE_H

#include <stdint.h>

typedef struct {
    uint8_t led_alarm_on;
    uint8_t led_alarm_off;
    uint8_t led_blue_on;
    uint8_t led_blue_off;
    uint8_t led_white_on;
    uint8_t led_white_off;
    uint8_t led_network_on;
    uint8_t led_network_off;
} Actuator_Cmds;

extern Actuator_Cmds actuator_cmds;

void put_ev_act_led_alarm_on(void);
void put_ev_act_led_alarm_off(void);
void put_ev_act_led_blue_on(void);
void put_ev_act_led_blue_off(void);
void put_ev_act_led_white_on(void);
void put_ev_act_led_white_off(void);
void put_ev_act_led_network_on(void);
void put_ev_act_led_network_off(void);

#endif /* TASK_ACTUATOR_INTERFACE_H */
