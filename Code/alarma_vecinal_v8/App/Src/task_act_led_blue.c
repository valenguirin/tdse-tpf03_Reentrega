/*
 * FSM del actuador LED azul (sirena)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_blue_on / off
   (ver task_actuator_interface.h)
 * La duración de la alarma la gestiona tick_sys en task_system.c.
 * Statechart (ver act_led_blue.png)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_act_led_blue.h"
#include "task_actuator_interface.h" //comandos del sistema
#include "bsp_gpio.h"                //para acceso al hardware

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del actuador LED azul:

typedef enum {
    ST_ACT_LED_BLUE_OFF ,   /* LED apagado,  esperando comando */
    ST_ACT_LED_BLUE_ON  ,   /* LED encendido                   */
} FSM_STATUS_ACT_LED_BLUE;

static FSM_STATUS_ACT_LED_BLUE fsm_led_blue = ST_ACT_LED_BLUE_OFF;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void act_led_blue_init(void)
{
    fsm_led_blue = ST_ACT_LED_BLUE_OFF;
    bsp_gpio_led_blue(0);   /* acción de entrada del estado inicial */
}

/* -------------------------------------------------------------------------------------*/

void act_led_blue_update(void) //FSM
{
    switch (fsm_led_blue)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_BLUE_OFF:
        if (actuator_cmds.led_blue_on) {
            actuator_cmds.led_blue_on = 0;
            bsp_gpio_led_blue(1);            /*  entrada: encender */
            fsm_led_blue = ST_ACT_LED_BLUE_ON;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_BLUE_ON:
        if (actuator_cmds.led_blue_off) {
            actuator_cmds.led_blue_off = 0;
            bsp_gpio_led_blue(0);            /*  entrada: apagar  */
            fsm_led_blue = ST_ACT_LED_BLUE_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        act_led_blue_init();
        break;
    }
}
