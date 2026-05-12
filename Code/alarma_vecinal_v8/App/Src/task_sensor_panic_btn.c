/*
 * FSM del botón de pánico con antirrebote de 50ms, timer y lógica.
 * Statechart (ver statechar_panic_button.ysc)
 * Se manda hacia la interface del sistema : ev_sys_panic_pressed / ev_sys_panic_released
   (ver task_system_interface.h)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_system_interface.h"
#include "bsp_gpio.h" //para acceso al hardware

/* -------------------------------------------------------------------------------------*/
#include "task_sensor_panic_btn.h"
#include <stdint.h>

/* -------------------------------------------------------------------------------------*/
// Constante de debounce 
#define DEBOUNCE_MS  50u    // ticks para confirmar un flanco (para esta aplicación :1 tick = 1 ms)

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del botón de pánico

typedef enum {
    ST_PANIC_BTN_UP      ,  /* botón sin presionar         */
    ST_PANIC_BTN_FALLING ,  /* flanco de bajada detectado  */
    ST_PANIC_BTN_DOWN    ,  /* botón presionadp            */
    ST_PANIC_BTN_RISING  ,  /* flanco de subida detectado  */
} FSM_STATUS_PANIC_BTN;

//Se declara una variable fsm_panic_btn de tipo enumerativo FSM_STATUS_PANIC_BTN
static FSM_STATUS_PANIC_BTN  fsm_panic_btn  = ST_PANIC_BTN_UP;
static uint32_t         tick_panic_btn = 0;



/* -------------------------------------------------------------------------------------*/
// APIs públicas
void sensor_panic_btn_init(void)
{
    fsm_panic_btn  = ST_PANIC_BTN_UP;
    tick_panic_btn = 0;
}

void sensor_panic_btn_update(void) //FSM
{
    uint8_t presionado = bsp_gpio_panic_btn_pressed();//se declara una variable de 1 byte, es como bool solo que universal

    switch (fsm_panic_btn)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_PANIC_BTN_UP:
        /*
         * Dos caminos para detectar el flanco:
         *   1. Polling (presionado): funciona siempre, requiere CPU activa.
         *   2. ISR flag (bsp_gpio_btn_panic_isr_triggered): más rápido, compatible
         *      con sleep/WFI — la EXTI despertará al micro y el flag
         *      estará a 1 cuando esta FSM se ejecute el siguiente tick.
         */
        if (presionado || bsp_gpio_panic_btn_isr_triggered()) {
            tick_panic_btn = DEBOUNCE_MS;
            fsm_panic_btn  = ST_PANIC_BTN_FALLING;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_PANIC_BTN_FALLING:
        if (!presionado) {
            fsm_panic_btn = ST_PANIC_BTN_UP;   /* era ruido, en el siguiente tick volver al estado anterior   */
        }
        else if (tick_panic_btn > 0) {
            tick_panic_btn--;                   /* contando...siguiente tick         */
        }
        else {
            put_event_task_system(EV_SYS_PANIC_BTN_PRESSED);  /*  presión confirmada */
            fsm_panic_btn = ST_PANIC_BTN_DOWN;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_PANIC_BTN_DOWN:
        if (!presionado) {
            tick_panic_btn = DEBOUNCE_MS;
            fsm_panic_btn  = ST_PANIC_BTN_RISING;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_PANIC_BTN_RISING:
        if (presionado) {
            fsm_panic_btn = ST_PANIC_BTN_DOWN;             /* era ruido, en el siguiente tick volver al estado anterior  */
        }
        else if (tick_panic_btn == 0) {
            put_event_task_system(EV_SYS_PANIC_BTN_RELEASED); /*liberación el bontón confirmada */
            fsm_panic_btn = ST_PANIC_BTN_UP;
        }
        else {
            tick_panic_btn--;                              /* contando... siguiente tick        */
        }
        break;
    /* ─────────────────────────────────────────────────────────── */
    default:
        sensor_panic_btn_init();
        break;
    }
}
