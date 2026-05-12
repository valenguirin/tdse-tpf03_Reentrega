/*
 * FSM principal del sistema con 8 estados
 * 8 estados: combinación de día/noche × red/sin red × alarma on/off.

 * Consume eventos de task_system_interface : EV_SYS_PANIC_BTN_PRESSED / EV_SYS_LDR_NIGHT / etc.
   (ver task_system_interface.h)
 * Manda comandos a task_actuator_interface y órdenes a gsm_interface.
   (ver task_actuator_interface.h y gsm_interface.h)
 * Statechart (ver system.ysc)

 */

/* -------------------------------------------------------------------------------------*/
#include "task_system.h"
#include "task_system_interface.h"   //cola de eventos entrantes
#include "task_actuator_interface.h" //comandos hacia los actuadores
#include "gsm_interface.h"           //órdenes hacia el módulo GSM
#include "ble_interface.h"           //órdenes hacia el módulo BLE (forzar corte de sesión)

/* -------------------------------------------------------------------------------------*/
// Implementación privada de la FSM del sistema:

typedef enum
{
    ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF,
    ST_SYS_DAY_NET_AVAILABLE_ALARM_OFF,
    ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_OFF,
    ST_SYS_NIGHT_NET_AVAILABLE_ALARM_OFF,
    ST_SYS_DAY_NET_UNAVAILABLE_ALARM_ON,
    ST_SYS_DAY_NET_AVAILABLE_ALARM_ON,
    ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_ON,
    ST_SYS_NIGHT_NET_AVAILABLE_ALARM_ON,
} FSM_STATUS_SYS;

static FSM_STATUS_SYS fsm_sys = ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF;
static uint32_t tick_sys = 0;

/* -------------------------------------------------------------------------------------*/
// Duración de la alarma:
#define ALARM_MS 5000u /* ticks hasta apagar alarma (1 tick = 1 ms) */

/* -------------------------------------------------------------------------------------*/
// Helpers privados de transición a alarma:

static void trigger_alarm_day(void)
{
    put_ev_act_led_alarm_off();
    put_ev_act_led_blue_on();
    put_event_task_ble(EV_BLE_FORCE_CLOSE); /* si había sesión BLE abierta, cortarla */
    tick_sys = ALARM_MS;
}

static void trigger_alarm_night(void)
{
    put_ev_act_led_alarm_off();
    put_ev_act_led_blue_on();
    put_ev_act_led_white_on();
    put_event_task_ble(EV_BLE_FORCE_CLOSE); /* si había sesión BLE abierta, cortarla */
    tick_sys = ALARM_MS;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void system_init(void)
{
    init_queue_event_task_system();
    fsm_sys = ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF;
    tick_sys = 0;

    /* acciones de entrada del estado inicial */
    put_ev_act_led_alarm_on();
    /* led_blue, led_white y led_network NO se comandan acá — sus init()
       ya los apagaron directo por BSP. Mandar off acá dejaría el flag
       led_*_off=1 sin consumir (el actuador solo lo lee estando en ON),
       y al primer encendido lo apagaría en el siguiente tick. */
}

/* -------------------------------------------------------------------------------------*/

void system_update(void) // FSM
{
    /*
     * Solo los estados ALARM_OFF atienden eventos.
     * Durante alarma activa (ALARM_ON) los eventos se descartan
     * Para esta aplicación, en 5 s vuelve al reposo y ahí los atenderá si llegan nuevos.
     */
    while (any_event_task_system())
    {
        task_system_ev_t ev = get_event_task_system();

        switch (fsm_sys)
        {

        /* ─────────────────────────────────────────────────────────── */
        case ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF:
            switch (ev)
            {
            case EV_SYS_LDR_NIGHT:
                fsm_sys = ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_GSM_ON_NETWORK:
                put_ev_act_led_network_on();
                fsm_sys = ST_SYS_DAY_NET_AVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_PANIC_BTN_PRESSED:
                trigger_alarm_day();
                fsm_sys = ST_SYS_DAY_NET_UNAVAILABLE_ALARM_ON;
                break;
            default:
                break;
            }
            break;

        /* ─────────────────────────────────────────────────────────── */
        case ST_SYS_DAY_NET_AVAILABLE_ALARM_OFF:
            switch (ev)
            {
            case EV_SYS_LDR_NIGHT:
                fsm_sys = ST_SYS_NIGHT_NET_AVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_GSM_OFF_NETWORK:
                put_ev_act_led_network_off();
                fsm_sys = ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_PANIC_BTN_PRESSED:
            case EV_SYS_CALL_AUTHORIZED:
                trigger_alarm_day();
                put_event_task_gsm(EV_GSM_SEND_SMS);
                fsm_sys = ST_SYS_DAY_NET_AVAILABLE_ALARM_ON;
                break;
            default:
                break;
            }
            break;

        /* ─────────────────────────────────────────────────────────── */
        case ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_OFF:
            switch (ev)
            {
            case EV_SYS_LDR_DAY:
                fsm_sys = ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_GSM_ON_NETWORK:
                put_ev_act_led_network_on();
                fsm_sys = ST_SYS_NIGHT_NET_AVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_PANIC_BTN_PRESSED:
                trigger_alarm_night();
                fsm_sys = ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_ON;
                break;
            default:
                break;
            }
            break;

        /* ─────────────────────────────────────────────────────────── */
        case ST_SYS_NIGHT_NET_AVAILABLE_ALARM_OFF:
            switch (ev)
            {
            case EV_SYS_LDR_DAY:
                fsm_sys = ST_SYS_DAY_NET_AVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_GSM_OFF_NETWORK:
                put_ev_act_led_network_off();
                fsm_sys = ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_OFF;
                break;
            case EV_SYS_PANIC_BTN_PRESSED:
            case EV_SYS_CALL_AUTHORIZED:
                trigger_alarm_night();
                put_event_task_gsm(EV_GSM_SEND_SMS);
                fsm_sys = ST_SYS_NIGHT_NET_AVAILABLE_ALARM_ON;
                break;
            default:
                break;
            }
            break;

        /* ─────────────────────────────────────────────────────────── */
        default:
            break; /* estados ALARM_ON descartan todos los eventos */
        }
    }

    /* Timer de alarma:corre cada tick independientemente de eventos */
    switch (fsm_sys)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_SYS_DAY_NET_UNAVAILABLE_ALARM_ON:
        if (tick_sys > 0)
        {
            tick_sys--;
        }
        else
        {
            put_ev_act_led_alarm_on();
            put_ev_act_led_blue_off();
            fsm_sys = ST_SYS_DAY_NET_UNAVAILABLE_ALARM_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_SYS_DAY_NET_AVAILABLE_ALARM_ON:
        if (tick_sys > 0)
        {
            tick_sys--;
        }
        else
        {
            put_ev_act_led_alarm_on();
            put_ev_act_led_blue_off();
            fsm_sys = ST_SYS_DAY_NET_AVAILABLE_ALARM_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_ON:
        if (tick_sys > 0)
        {
            tick_sys--;
        }
        else
        {
            put_ev_act_led_alarm_on();
            put_ev_act_led_blue_off();
            put_ev_act_led_white_off();
            fsm_sys = ST_SYS_NIGHT_NET_UNAVAILABLE_ALARM_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_SYS_NIGHT_NET_AVAILABLE_ALARM_ON:
        if (tick_sys > 0)
        {
            tick_sys--;
        }
        else
        {
            put_ev_act_led_alarm_on();
            put_ev_act_led_blue_off();
            put_ev_act_led_white_off();
            fsm_sys = ST_SYS_NIGHT_NET_AVAILABLE_ALARM_OFF;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        break; /* estados ALARM_OFF no decrementan el timer */
    }
}
