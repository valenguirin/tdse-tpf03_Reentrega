/*
 * Cola circular de eventos para el módulo BLE.
 * Escritura: task_system.c. Lectura: ble.c.
 */

/* -------------------------------------------------------------------------------------*/
#include "ble_interface.h"

/* -------------------------------------------------------------------------------------*/
#define EVENT_UNDEFINED ((task_ble_ev_t)255u)
#define MAX_EVENTS 8u

/* -------------------------------------------------------------------------------------*/
static struct
{
    uint32_t head;  /* siguiente slot a escribir  */
    uint32_t tail;  /* siguiente slot a leer    */
    uint32_t count; /* eventos pendientes      */
    task_ble_ev_t queue[MAX_EVENTS];
} queue_task_ble;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void init_queue_event_task_ble(void)
{
    uint32_t i;
    queue_task_ble.head = 0;
    queue_task_ble.tail = 0;
    queue_task_ble.count = 0;
    for (i = 0; i < MAX_EVENTS; i++)
    {
        queue_task_ble.queue[i] = EVENT_UNDEFINED;
    }
}

void put_event_task_ble(task_ble_ev_t event)
{
    if (queue_task_ble.count == MAX_EVENTS) return;   /* cola llena: descarta sin corromper */
    queue_task_ble.count++;
    queue_task_ble.queue[queue_task_ble.head++] = event;
    if (MAX_EVENTS == queue_task_ble.head)
    {
        queue_task_ble.head = 0;
    }
}

task_ble_ev_t get_event_task_ble(void)
{
    task_ble_ev_t event;
    queue_task_ble.count--;
    event = queue_task_ble.queue[queue_task_ble.tail];
    queue_task_ble.queue[queue_task_ble.tail++] = EVENT_UNDEFINED;
    if (MAX_EVENTS == queue_task_ble.tail)
    {
        queue_task_ble.tail = 0;
    }
    return event;
}

uint8_t any_event_task_ble(void)
{
    return (queue_task_ble.head != queue_task_ble.tail); /* head==tail, entonces vacía */
}
