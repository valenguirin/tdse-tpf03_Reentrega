/*
 * FSM del actuador LED blanco (estrobo)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_white_on / off
   (ver task_actuator_interface.h)
 * Solo se activa en alarma nocturna. Mismo patrón que task_act_led_blue.c.
 * Statechart (ver act_led_white.png)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_act_led_white.h"
#include "task_actuator_interface.h" //comandos del sistema
#include "bsp_gpio.h"                //para acceso al hardware

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del actuador LED blanco:

typedef enum {
    ST_ACT_LED_WHITE_OFF ,   /* LED apagado,  esperando comando */
    ST_ACT_LED_WHITE_ON  ,   /* LED encendido                   */
} FSM_STATUS_ACT_LED_WHITE;

static FSM_STATUS_ACT_LED_WHITE fsm_led_white = ST_ACT_LED_WHITE_OFF;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void act_led_white_init(void)
{
    fsm_led_white = ST_ACT_LED_WHITE_OFF;
    bsp_gpio_led_white(0);   /* acción de entrada del estado inicial */
}

/* -------------------------------------------------------------------------------------*/

void act_led_white_update(void) //FSM
{
    switch (fsm_led_white)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_WHITE_OFF:
        if (actuator_cmds.led_white_on) {
            actuator_cmds.led_white_on = 0;
            bsp_gpio_led_white(1);           /*  entrada: encender */
            fsm_led_white = ST_ACT_LED_WHITE_ON;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_WHITE_ON:
        if (actuator_cmds.led_white_off) {
            actuator_cmds.led_white_off = 0;
            bsp_gpio_led_white(0);           /* entrada: apagar  */
            fsm_led_white = ST_ACT_LED_WHITE_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        act_led_white_init();
        break;
    }
}
