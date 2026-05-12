/*
 * Se presenta la información pública del módulo botón de pánico

 * Se manda hacia la interface del sistema : ev_sys_panic_pressed / ev_sys_panic_released
   (ver task_system_interface.h)

 * Solo se expone lo que el resto del sistema necesita saber,
   cómo inicializar el módulo y cómo actualizarlo para cada tick.

 * Todo lo interno (estados, timer y  lógica) vive en el .c.
 */

#ifndef TASK_SENSOR_PANIC_BTN_H
#define TASK_SENSOR_PANIC_BTN_H

void sensor_panic_btn_init(void);     /* se llama una vez en app_init()     */
void sensor_panic_btn_update(void);   /* se llama cada tick en app_update() */

#endif 
