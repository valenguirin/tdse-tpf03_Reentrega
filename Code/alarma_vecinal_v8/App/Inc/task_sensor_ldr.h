/*
 * Se presenta la información pública del sensor LDR (día/noche)
 * Se manda hacia la interface del sistema : ev_sys_ldr_day / ev_sys_ldr_night
   (ver task_system_interface.h)
 * Solo se expone lo que el resto del sistema necesita saber,
   cómo inicializar el módulo y cómo actualizarlo para cada tick.
 * Todo lo interno (estados, timer y lógica) vive en el .c.
 */

#ifndef TASK_SENSOR_LDR_H
#define TASK_SENSOR_LDR_H

void sensor_ldr_init(void);     /* llamar una vez en app_init()     */
void sensor_ldr_update(void);   /* llamar cada tick en app_update() */

#endif
