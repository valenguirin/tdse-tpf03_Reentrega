/*
 * Cola de eventos hacia el módulo EEPROM.
 * FIFO circular de MAX_EVENTS posiciones.
 * Escribe config_neighbourhood.c (en cada caller_add/remove exitoso);
 * lee eeprom.c.
 */

#ifndef EEPROM_INTERFACE_H
#define EEPROM_INTERFACE_H

#include <stdint.h>

typedef enum
{
    EV_EEPROM_PERSIST_WHITELIST, /* hubo un caller_add/remove -> grabar a EEPROM */
} task_eeprom_ev_t;

void init_queue_event_task_eeprom(void); /* se llama una vez en eeprom_init() */
void put_event_task_eeprom(task_eeprom_ev_t event);
task_eeprom_ev_t get_event_task_eeprom(void);
uint8_t any_event_task_eeprom(void); /* 1 = hay eventos pendientes        */

#endif
