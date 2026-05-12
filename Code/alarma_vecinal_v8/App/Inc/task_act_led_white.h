/*
 * Se presenta la información pública del actuador LED blanco (estrobo)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_white_on / off
   (ver task_actuator_interface.h)
 * Solo se activa en alarma nocturna.
 * Solo se expone lo que el resto del sistema necesita saber,
   cómo inicializar el módulo y cómo actualizarlo para cada tick.
 * Todo lo interno (estados y lógica) vive en el .c.
 */

#ifndef TASK_ACT_LED_WHITE_H
#define TASK_ACT_LED_WHITE_H

void act_led_white_init(void);    /* se llama una vez en app_init()     */
void act_led_white_update(void);  /* se llama cada tick en app_update() */

#endif
