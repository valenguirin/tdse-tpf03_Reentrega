/*
 * Cola circular de eventos para el módulo GSM.
 * Escritura: task_system.c. Lectura: gsm.c.
 */

/* -------------------------------------------------------------------------------------*/
#include "gsm_interface.h"

/* -------------------------------------------------------------------------------------*/
#define EVENT_UNDEFINED ((task_gsm_ev_t)255u) /* centinela para slots libres */
#define MAX_EVENTS 8u

/* -------------------------------------------------------------------------------------*/
static struct
{
    uint32_t head;  /* siguiente slot a escribir      */
    uint32_t tail;  /* siguiente slot a leer          */
    uint32_t count; /* eventos pendientes             */
    task_gsm_ev_t queue[MAX_EVENTS];
} queue_task_gsm;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void init_queue_event_task_gsm(void)
{
    uint32_t i;
    queue_task_gsm.head = 0;
    queue_task_gsm.tail = 0;
    queue_task_gsm.count = 0;
    for (i = 0; i < MAX_EVENTS; i++)
    {
        queue_task_gsm.queue[i] = EVENT_UNDEFINED;
    }
}

void put_event_task_gsm(task_gsm_ev_t event)
{
    if (queue_task_gsm.count == MAX_EVENTS)
        return; /* cola llena: descarta sin corromper */
    queue_task_gsm.count++;
    queue_task_gsm.queue[queue_task_gsm.head++] = event;
    if (MAX_EVENTS == queue_task_gsm.head)
    {
        queue_task_gsm.head = 0; /* wrap-around */
    }
}

task_gsm_ev_t get_event_task_gsm(void)
{
    task_gsm_ev_t event;
    queue_task_gsm.count--;
    event = queue_task_gsm.queue[queue_task_gsm.tail];
    queue_task_gsm.queue[queue_task_gsm.tail++] = EVENT_UNDEFINED;
    if (MAX_EVENTS == queue_task_gsm.tail)
    {
        queue_task_gsm.tail = 0; /* wrap-around */
    }
    return event;
}

uint8_t any_event_task_gsm(void)
{
    return (queue_task_gsm.head != queue_task_gsm.tail); /* head==tail -> vacía */
}
