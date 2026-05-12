/*
 * Cola circular de eventos para el módulo EEPROM.
 * Escritura: config_neighbourhood.c (en cada caller_add/remove).
 * Lectura  : eeprom.c.
 */

/* -------------------------------------------------------------------------------------*/
#include "eeprom_interface.h"

/* -------------------------------------------------------------------------------------*/
#define EVENT_UNDEFINED ((task_eeprom_ev_t)255u) /* centinela para slots libres */
#define MAX_EVENTS 4u

/* -------------------------------------------------------------------------------------*/
static struct
{
    uint32_t head;  /* siguiente slot a escribir      */
    uint32_t tail;  /* siguiente slot a leer          */
    uint32_t count; /* eventos pendientes             */
    task_eeprom_ev_t queue[MAX_EVENTS];
} queue_task_eeprom;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void init_queue_event_task_eeprom(void)
{
    uint32_t i;
    queue_task_eeprom.head = 0;
    queue_task_eeprom.tail = 0;
    queue_task_eeprom.count = 0;
    for (i = 0; i < MAX_EVENTS; i++)
    {
        queue_task_eeprom.queue[i] = EVENT_UNDEFINED;
    }
}

void put_event_task_eeprom(task_eeprom_ev_t event)
{
    if (queue_task_eeprom.count == MAX_EVENTS)
        return; /* cola llena: descarta sin corromper */
    queue_task_eeprom.count++;
    queue_task_eeprom.queue[queue_task_eeprom.head++] = event;
    if (MAX_EVENTS == queue_task_eeprom.head)
    {
        queue_task_eeprom.head = 0; /* wrap-around */
    }
}

task_eeprom_ev_t get_event_task_eeprom(void)
{
    task_eeprom_ev_t event;
    queue_task_eeprom.count--;
    event = queue_task_eeprom.queue[queue_task_eeprom.tail];
    queue_task_eeprom.queue[queue_task_eeprom.tail++] = EVENT_UNDEFINED;
    if (MAX_EVENTS == queue_task_eeprom.tail)
    {
        queue_task_eeprom.tail = 0; /* wrap-around */
    }
    return event;
}

uint8_t any_event_task_eeprom(void)
{
    return (queue_task_eeprom.head != queue_task_eeprom.tail); /* head==tail, significa  vacía */
}
