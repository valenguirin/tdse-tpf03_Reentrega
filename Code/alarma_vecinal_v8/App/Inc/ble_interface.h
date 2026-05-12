/*
 * Cola de eventos del sistema hacia el módulo BLE.
 * FIFO circular de MAX_EVENTS posiciones.
 * Escribe task_system.c con put_event_task_ble; lee ble.c con get_event_task_ble.
 */

#ifndef BLE_INTERFACE_H
#define BLE_INTERFACE_H

#include <stdint.h>

typedef enum {
    EV_BLE_FORCE_CLOSE,    /* sistema ordena desconectar al peer (alarma activada) */
} task_ble_ev_t;

void          init_queue_event_task_ble(void);   /* se llama una vez en ble_init() */
void          put_event_task_ble       (task_ble_ev_t event);
task_ble_ev_t get_event_task_ble       (void);
uint8_t       any_event_task_ble       (void);   /* 1 = hay eventos pendientes     */

#endif
