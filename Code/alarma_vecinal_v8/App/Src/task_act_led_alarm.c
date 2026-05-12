/*
 * FSM del actuador LED ROJO (estado alarma)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_alarm_on / off
   (ver task_actuator_interface.h)
 * Statechart (ver act_led_alarm.png)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_act_led_alarm.h"
#include "task_actuator_interface.h" //comandos del sistema
#include "bsp_gpio.h"                //para acceso al hardware

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del actuador LED rojo:

typedef enum {
    ST_ACT_LED_ALARM_OFF ,   /* LED apagado, alarma activa o sin iniciar  */
    ST_ACT_LED_ALARM_ON  ,   /* LED encendido, sistema armado, en espera   */
} FSM_STATUS_ACT_LED_ALARM;

static FSM_STATUS_ACT_LED_ALARM fsm_led_alarm = ST_ACT_LED_ALARM_OFF;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void act_led_alarm_init(void)
{
    fsm_led_alarm = ST_ACT_LED_ALARM_OFF;
    bsp_gpio_led_alarm(0);   /* acción de entrada del estado inicial */
}

/* -------------------------------------------------------------------------------------*/

void act_led_alarm_update(void) //FSM
{
    switch (fsm_led_alarm)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_ALARM_OFF:
        if (actuator_cmds.led_alarm_on) {
            actuator_cmds.led_alarm_on = 0;
            bsp_gpio_led_alarm(1);           /*  entrada: encender */
            fsm_led_alarm = ST_ACT_LED_ALARM_ON;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_ALARM_ON:
        if (actuator_cmds.led_alarm_off) {
            actuator_cmds.led_alarm_off = 0;
            bsp_gpio_led_alarm(0);           /*  entrada: apagar  */
            fsm_led_alarm = ST_ACT_LED_ALARM_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        act_led_alarm_init();
        break;
    }
}
