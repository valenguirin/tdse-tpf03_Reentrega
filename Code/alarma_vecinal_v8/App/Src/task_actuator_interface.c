/*
 * Bus de comandos del sistema hacia los actuadores (LEDs)
 * Cada put_ev_act_*() setea el flag correspondiente en actuator_cmds.
 * Cada actuador lo leerá y consumirá en su propio update().
   (ver task_act_led_*.c)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_actuator_interface.h"

/* -------------------------------------------------------------------------------------*/
Actuator_Cmds actuator_cmds = {0};

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void put_ev_act_led_alarm_on(void)    { actuator_cmds.led_alarm_on    = 1; }
void put_ev_act_led_alarm_off(void)   { actuator_cmds.led_alarm_off   = 1; }
void put_ev_act_led_blue_on(void)     { actuator_cmds.led_blue_on     = 1; }
void put_ev_act_led_blue_off(void)    { actuator_cmds.led_blue_off    = 1; }
void put_ev_act_led_white_on(void)    { actuator_cmds.led_white_on    = 1; }
void put_ev_act_led_white_off(void)   { actuator_cmds.led_white_off   = 1; }
void put_ev_act_led_network_on(void)  { actuator_cmds.led_network_on  = 1; }
void put_ev_act_led_network_off(void) { actuator_cmds.led_network_off = 1; }
