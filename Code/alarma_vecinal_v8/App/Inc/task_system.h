/*
 * Se presenta la información pública de la FSM principal del sistema
 * FSM de 8 estados: combinación de día/noche × red/sin red × alarma on/off.
 * Consume eventos de task_system_interface y manda comandos a task_actuator_interface.
   (ver task_system_interface.h y task_actuator_interface.h)
 * Solo se expone lo que app.c necesita saber.
 * Todo lo interno (estados, lógica, timer) vive en el .c.
 */

#ifndef TASK_SYSTEM_H
#define TASK_SYSTEM_H

void system_init(void);   /* se llama una vez en app_init()     */
void system_update(void); /* se llama cada tick en app_update() */

#endif
