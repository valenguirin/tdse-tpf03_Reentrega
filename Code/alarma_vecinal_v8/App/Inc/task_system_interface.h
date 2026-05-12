/*
 * Cola de eventos de sensores y módulos hacia la FSM del sistema
 * Los sensores (panic_btn, ldr) y módulos (gsm, ble) publican con put_event_task_system().
 * La FSM del sistema los consume con get_event_task_system().
   (ver task_system.c)
 * Implementación: cola circular FIFO de MAX_EVENTS posiciones,
   los eventos no se pierden entre ticks y se procesan en orden.
 */

#ifndef TASK_SYSTEM_INTERFACE_H
#define TASK_SYSTEM_INTERFACE_H

#include <stdint.h>

typedef enum {
    EV_SYS_PANIC_BTN_PRESSED ,   /* botón presionado (confirmado 50 ms)        */
    EV_SYS_PANIC_BTN_RELEASED,   /* botón liberado   (confirmado 50 ms)        */
    EV_SYS_LDR_NIGHT         ,   /* oscureció        (confirmado 1000 ms)      */
    EV_SYS_LDR_DAY           ,   /* amaneció         (confirmado 1000 ms)      */
    EV_SYS_GSM_ON_NETWORK    ,   /* GSM registrado   (AT+CREG stat=1 o 5)      */
    EV_SYS_GSM_OFF_NETWORK   ,   /* GSM sin red      (AT+CREG stat=otro)       */
    EV_SYS_CALL_AUTHORIZED   ,   /* llamada entrante de número en whitelist    */
    EV_SYS_BLE_USER_CONNECTED,   /* HM-10 reportó OK+CONN — alguien pareó      */
    EV_SYS_BLE_USER_AUTHED   ,   /* user+pass válidos — sesión activa          */
    EV_SYS_BLE_USER_DISCONNECTED,/* HM-10 reportó OK+LOST o cerramos forzado   */
} task_system_ev_t;

void             init_queue_event_task_system(void);
void             put_event_task_system(task_system_ev_t event);
task_system_ev_t get_event_task_system(void);
uint8_t          any_event_task_system(void);

#endif
