/*
 * Se presenta la información pública del actuador LED rojo (estado alarma)
 * Recibe comandos desde task_actuator_interface : actuator_cmds.led_alarm_on / off
   (ver task_actuator_interface.h)
 * LED encendido  -> sistema armado y en espera (alarma lista)
 * LED apagado    -> alarma activada, en curso
 * Solo se expone lo que el resto del sistema necesita saber,
   cómo inicializar el módulo y cómo actualizarlo para cada tick.
 * Todo lo interno (estados y lógica) vive en el .c.
 */

#ifndef APP_ACT_LED_ALARM_H
#define APP_ACT_LED_ALARM_H

void act_led_alarm_init(void);   /* se llama una vez en app_init()     */
void act_led_alarm_update(void); /* se llama cada tick en app_update() */

#endif
