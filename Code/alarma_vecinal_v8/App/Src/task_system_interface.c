/*
 * Cola circular de eventos para la FSM del sistema
 * Gracias a la implementación FIFO de MAX_EVENTS posiciones, los eventos persisten entre ticks
   hasta que el sistema los consuma.
 * Escritura: put_event_task_system(), esta función es llamada por sensores y módulos gsm/ble.
 * Lectura:   get_event_task_system(), esta función es llamada únicamente por task_system.c.

 */

/* -------------------------------------------------------------------------------------*/
#include "task_system_interface.h"

/* -------------------------------------------------------------------------------------*/
#define EVENT_UNDEFINED  ((task_system_ev_t)255u)//valor de 255 a un evento no definido
#define MAX_EVENTS       16u

/* -------------------------------------------------------------------------------------*/
static struct {
    uint32_t         head;
    uint32_t         tail;
    uint32_t         count;
    task_system_ev_t queue[MAX_EVENTS];
} queue_task_system;

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

// se inicializa la queue
void init_queue_event_task_system(void)
{
    uint32_t i;
    queue_task_system.head  = 0;
    queue_task_system.tail  = 0;
    queue_task_system.count = 0;
    for (i = 0; i < MAX_EVENTS; i++)
        queue_task_system.queue[i] = EVENT_UNDEFINED;
}




/*se añade un evento nuevo a la queue*/
void put_event_task_system(task_system_ev_t event)
{
    if (queue_task_system.count == MAX_EVENTS) return;   /* cola llena: descarta sin corromper */
    queue_task_system.count++;
    queue_task_system.queue[queue_task_system.head++] = event;
    if (MAX_EVENTS == queue_task_system.head)
        queue_task_system.head = 0;
}



/*Resta el la cantidad de eventos sin antender en 1,
 * mueve una posición a la derecha el tail, por lo que deja listo
   para en el siguiente llamdado se consuma el evento que siga*/
task_system_ev_t get_event_task_system(void)
{
    task_system_ev_t event;
    queue_task_system.count--;
    event = queue_task_system.queue[queue_task_system.tail];
    queue_task_system.queue[queue_task_system.tail++] = EVENT_UNDEFINED;
    if (MAX_EVENTS == queue_task_system.tail)
        queue_task_system.tail = 0;
    return event;
}


//El resultado es 1 si head y tail son distintos, lo que implica que sí
//hay nuevos eventos sin atender
uint8_t any_event_task_system(void)
{
    return (queue_task_system.head != queue_task_system.tail);
}
