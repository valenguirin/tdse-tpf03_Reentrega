/*
 * Se presenta la información pública del actuador LED azul (sirena)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_blue_on / off
   (ver task_actuator_interface.h)
 * Solo se expone lo que el resto del sistema necesita saber,
   cómo inicializar el módulo y cómo actualizarlo para cada tick.
 * Todo lo interno (estados y lógica) vive en el .c.
 */

#ifndef TASK_ACT_LED_BLUE_H
#define TASK_ACT_LED_BLUE_H

void act_led_blue_init(void);   /* se llama una vez en app_init()     */
void act_led_blue_update(void); /* se llama cada tick en app_update() */

#endif
