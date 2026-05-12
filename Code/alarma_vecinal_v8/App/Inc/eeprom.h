/*
 * Módulo EEPROM (24LC256 en I2C1).
 * FSM de 6 estados que persiste la whitelist en la EEPROM externa para
 * que sobreviva al apagado.
 *
 * eeprom_init() es bloqueante (válido pre-scheduler): lee el header,
 * valida magic + version + crc16 y, si es válido, sobrescribe la whitelist
 * en RAM. Si está corrupto o vacío, encola PERSIST para grabar la seed
 * en el primer tick del cyclic executive.
 *
 * Ojo con el stack: durante la carga se reservan ~800 B temporales en stack
 * (el buffer de los 50 entries de la whitelist). Funciona porque init corre
 * antes del cyclic executive y el stack está vacío. NO usar esta función en
 * runtime — si algún día se necesita recargar la whitelist en caliente, hay
 * que mover ese buffer a .bss antes de llamarla.
 *
 * Es silencioso, no manda eventos. Consume EV_EEPROM_PERSIST_WHITELIST
 * (ver eeprom_interface.h).
 */

#ifndef EEPROM_H
#define EEPROM_H

void eeprom_init  (void);   /* bloqueante, se llama una vez en app_init()  */
void eeprom_update(void);   /* FSM, se llama cada tick en app_update       */

#endif
