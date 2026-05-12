/*
 * Configuración del sitio de instalación de la alarma vecinal.
 *
 * Para deployar en otra ubicación: editar solo config_neighbourhood.c
 *   - callers_seed[]   : números seed (se sobrescriben con la EEPROM al boot)
 *   - sms_recipients[] : números que reciben el SMS de alerta
 *   - SMS_MESSAGE      : texto del mensaje (puede ser un link de ubicación)
 *   - BLE_USERNAME     : usuario admin para sesión BLE
 *   - BLE_PASSWORD     : contraseña admin para sesión BLE
 *
 * Whitelist en runtime: arranca con callers_seed; si la EEPROM tiene un
 * bloque válido, eeprom_init() la sobrescribe con lo persistido. Cada
 * caller_add/remove encola PERSIST hacia la FSM del eeprom para grabar
 * los cambios asincrónicamente.
 *
 * El resto del sistema (gsm.c) no necesita cambios al cambiar de sitio.
 */

#ifndef CONFIG_NEIGHBOURHOOD_H
#define CONFIG_NEIGHBOURHOOD_H

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------------------*/
// Capacidades:

#define NEIGHBOURHOOD_WHITELIST_MAX 50u /* máximo de números autorizados en runtime  */
#define NEIGHBOURHOOD_PHONE_LEN_MAX 13u /* dígitos sin '+' (10 local, 13 internac.)  */

/* -------------------------------------------------------------------------------------*/
// Init : llamar una vez en app_init() antes de cualquier otra API:

void neighbourhood_init(void);

/* -------------------------------------------------------------------------------------*/
// Whitelist de llamadores (consumida por gsm.c, editada por ble.c):

bool neighbourhood_caller_authorized(const char *numero);
uint8_t neighbourhood_caller_count(void);
const char *neighbourhood_caller_get(uint8_t idx);

/*
 * caller_add : 0 = OK, 1 = ya existe, 2 = lista llena, 3 = número inválido
 * caller_remove : 0 = OK, 1 = no encontrado, 2 = número inválido
 *
 * "número inválido" = NULL, vacío, longitud fuera de rango o caracteres no dígitos.
 */
uint8_t neighbourhood_caller_add(const char *numero);
uint8_t neighbourhood_caller_remove(const char *numero);

/* -------------------------------------------------------------------------------------*/
// APIs internas usadas SOLO por eeprom.c al cargar la whitelist desde EEPROM.
// No usar desde la lógica de la app : saltean la validación y NO encolan
// el evento PERSIST

void neighbourhood_whitelist_clear(void);                          /* deja count = 0  */
void neighbourhood_whitelist_append_unchecked(const char *numero); /* sin validar    */

/* -------------------------------------------------------------------------------------*/
// Destinatarios SMS (consumido por gsm.c):

uint8_t neighbourhood_sms_count(void);
const char *neighbourhood_sms_recipient(uint8_t idx);
const char *neighbourhood_sms_message(void);

/* -------------------------------------------------------------------------------------*/
// Credenciales BLE (consumidas por ble.c):

const char *neighbourhood_ble_username(void);
const char *neighbourhood_ble_password(void);

#endif /* CONFIG_NEIGHBOURHOOD_H */
