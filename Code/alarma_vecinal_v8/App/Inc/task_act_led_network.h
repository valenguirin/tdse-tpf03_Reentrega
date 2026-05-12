/*
 * Se presenta la información pública del actuador LED amarillo (red GSM)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_network_on / off
   (ver task_actuator_interface.h)
 * LED encendido, si hay red GSM disponible
 * LED apagado, sin red (buscando, denegado, no registrado)
 * Solo se expone lo que el resto del sistema necesita saber,
   cómo inicializar el módulo y cómo actualizarlo para cada tick.
 * Todo lo interno (estados y lógica) vive en el .c.
 */

#ifndef TASK_ACT_LED_NETWORK_H
#define TASK_ACT_LED_NETWORK_H

void act_led_network_init(void);    /* se llama una vez en app_init()     */
void act_led_network_update(void);  /* se llama cada tick en app_update() */

#endif
