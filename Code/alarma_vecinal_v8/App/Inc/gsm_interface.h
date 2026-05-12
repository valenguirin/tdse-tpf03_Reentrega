/*
 * Cola de eventos del sistema hacia el módulo GSM.
 * FIFO circular de MAX_EVENTS posiciones.
 * Escribe task_system.c con put_event_task_gsm; lee gsm.c con get_event_task_gsm.
 */

#ifndef GSM_INTERFACE_H
#define GSM_INTERFACE_H

#include <stdint.h>

typedef enum {
    EV_GSM_SEND_SMS,    /* sistema ordena enviar SMS de alerta a los destinatarios */
} task_gsm_ev_t;

void          init_queue_event_task_gsm(void);   /* se llama una vez en gsm_init() */
void          put_event_task_gsm       (task_gsm_ev_t event);
task_gsm_ev_t get_event_task_gsm       (void);
uint8_t       any_event_task_gsm       (void);   /* 1 = hay eventos pendientes     */

#endif
