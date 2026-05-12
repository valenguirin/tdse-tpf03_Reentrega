/*
 * FSM del actuador LED amarillo (red GSM)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_network_on / off
   (ver task_actuator_interface.h)
 * Statechart (ver act_led_network.png)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_act_led_network.h"
#include "task_actuator_interface.h" //comandos del sistema
#include "bsp_gpio.h"                //para acceso al hardware

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del actuador LED amarillo:

typedef enum {
    ST_ACT_LED_NET_OFF ,   /* LED apagado  — sin red           */
    ST_ACT_LED_NET_ON  ,   /* LED encendido — red disponible   */
} FSM_STATUS_ACT_LED_NETWORK;

static FSM_STATUS_ACT_LED_NETWORK fsm_led_net = ST_ACT_LED_NET_OFF;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void act_led_network_init(void)
{
    fsm_led_net = ST_ACT_LED_NET_OFF;
    bsp_gpio_led_network(0);   /* acción de entrada del estado inicial */
}

/* -------------------------------------------------------------------------------------*/

void act_led_network_update(void) //FSM
{
    switch (fsm_led_net)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_NET_OFF:
        if (actuator_cmds.led_network_on) {
            actuator_cmds.led_network_on = 0;
            bsp_gpio_led_network(1);         /* ★ entrada: encender */
            fsm_led_net = ST_ACT_LED_NET_ON;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_ACT_LED_NET_ON:
        if (actuator_cmds.led_network_off) {
            actuator_cmds.led_network_off = 0;
            bsp_gpio_led_network(0);         /* ★ entrada: apagar  */
            fsm_led_net = ST_ACT_LED_NET_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        act_led_network_init();
        break;
    }
}
