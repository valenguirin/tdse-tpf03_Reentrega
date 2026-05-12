/*
 * FSM del sensor LDR con debounce de 1 segundo
 * El debounce de 1000 ms evita transiciones falsas por nubes pasajeras,
   sombras momentáneas o parpadeos de luz.
 * Statechart (ver sensor_ldr.ysc)

 */
/* -------------------------------------------------------------------------------------*/
#include "task_system_interface.h"
#include "bsp_gpio.h" //para acceso al hardware




/* -------------------------------------------------------------------------------------*/
#include "task_sensor_ldr.h"
#include <stdint.h>


/* -------------------------------------------------------------------------------------*/
// Constante de debounce:
#define DEBOUNCE_LDR_MS  1000u   /* ticks para confirmar un cambio de luz */

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del sensor ldr:
typedef enum {
    ST_LDR_DAY         ,   /* es de día,  estable                      */
    ST_LDR_GOING_NIGHT ,   /* detectó oscuridad, confirmando...        */
    ST_LDR_NIGHT       ,   /* es de noche, estable                     */
    ST_LDR_GOING_DAY   ,   /* detectó claridad, confirmando...         */
} FSM_STATUS_SENS_LDR;

static FSM_STATUS_SENS_LDR fsm_ldr  = ST_LDR_DAY;   /* arranca asumiendo que es de día */
static uint32_t  tick_ldr = 0;



/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void sensor_ldr_init(void)
{
    fsm_ldr  = ST_LDR_DAY;
    tick_ldr = 0;
}

/* -------------------------------------------------------------------------------------*/

void sensor_ldr_update(void)
{
    uint8_t noche = bsp_gpio_ldr_is_night();   /* 1 = oscuro, 0 = claro */

    switch (fsm_ldr)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_LDR_DAY:
        if (noche) {
            tick_ldr = DEBOUNCE_LDR_MS;
            fsm_ldr  = ST_LDR_GOING_NIGHT;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_LDR_GOING_NIGHT:
        if (!noche) {
            fsm_ldr = ST_LDR_DAY;          /* volvió la luz — era ruido*/
        }
        else if (tick_ldr > 0) {
            tick_ldr--;                     /* contando...*/
        }
        else {
            put_event_task_system(EV_SYS_LDR_NIGHT);   /* noche confirmada */
            fsm_ldr = ST_LDR_NIGHT;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_LDR_NIGHT:
        if (!noche) {
            tick_ldr = DEBOUNCE_LDR_MS;
            fsm_ldr  = ST_LDR_GOING_DAY;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_LDR_GOING_DAY:
        if (noche) {
            fsm_ldr = ST_LDR_NIGHT;        /* volvió la oscuridad, era ruido */
        }
        else if (tick_ldr > 0) {
            tick_ldr--;                     /* contando...*/
        }
        else {
            put_event_task_system(EV_SYS_LDR_DAY);     /* día confirmado */
            fsm_ldr = ST_LDR_DAY;
        }
        break;
    /* ─────────────────────────────────────────────────────────── */
    default:
        sensor_ldr_init();
        break;
    }
}
