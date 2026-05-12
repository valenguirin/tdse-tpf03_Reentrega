/*
 * Configuración del vecindario:se debe editar este archivo para cada nueva instalación.
 * El resto del sistema no requiere cambios al cambiar de ubicación.
 */

/* -------------------------------------------------------------------------------------*/
#include "config_neighbourhood.h"
#include "eeprom_interface.h" //para encolar PERSIST_WHITELIST en cada add/remove
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------------------*/
// Whitelist seed : números autorizados a activar la alarma mediante llamada telefónica.
// Formato: número local sin código de país (gsm.c quita "+54" del +CLIP: antes de comparar).
// Esta lista se copia a la whitelist editable en RAM al ejecutar neighbourhood_init().

static const char *const callers_seed[] = {
    //"1122334455", /* vecino 1 */
    //"1133445566", /* vecino 2 */
};

#define CALLERS_SEED_COUNT ((uint8_t)(sizeof(callers_seed) / sizeof(callers_seed[0])))

/* -------------------------------------------------------------------------------------*/
// Números que reciben el SMS de alerta cuando se activa la alarma.

static const char *const sms_recipients[] = {
    //"1155794099", /* sucursal 1 */
    "1126332726", /* sucursal 2 */
    "1133588475", /* policía    */
};
#define SMS_COUNT ((uint8_t)(sizeof(sms_recipients) / sizeof(sms_recipients[0])))

/* -------------------------------------------------------------------------------------*/
// Mensaje de alerta : link de ubicación (máx 160 caracteres).

static const char SMS_MESSAGE[] =
    "ALARMA ACTIVADA EN:https://www.google.com/maps/dir/?api=1&destination=-34.661042,-58.868015";

/* -------------------------------------------------------------------------------------*/
// Credenciales para sesión BLE (ver ble.c ahí se valida login de doble factor).
//   USERNAME : insensible a mayúsculas (admin == ADmIn == ADMIN).
//   PASSWORD : sensible a mayúsculas.

static const char BLE_USERNAME[] = "admin";
static const char BLE_PASSWORD[] = "FIUBA";

/* -------------------------------------------------------------------------------------*/
// Whitelist en RAM : editable desde BLE. Se inicializa con callers_seed[] en
// neighbourhood_init() y se modifica con caller_add / caller_remove. No persiste
// (al reset vuelve a la seed del .c).

static char whitelist[NEIGHBOURHOOD_WHITELIST_MAX][NEIGHBOURHOOD_PHONE_LEN_MAX + 1u];
static uint8_t whitelist_count;

/* -------------------------------------------------------------------------------------*/
// Helpers privados de validación de números:

static uint8_t is_valid_phone(const char *s)
// Acepta 10 a NEIGHBOURHOOD_PHONE_LEN_MAX dígitos puros. Sin '+', sin espacios.
{
    uint8_t i = 0u;
    if (s == NULL)
        return 0u;
    while (s[i] != '\0')
    {
        if (s[i] < '0' || s[i] > '9')
            return 0u;
        if (i >= NEIGHBOURHOOD_PHONE_LEN_MAX)
            return 0u;
        i++;
    }
    return (i >= 10u) ? 1u : 0u;
}

static int find_in_whitelist(const char *numero)
// Retorna índice si encuentra match exacto, -1 si no está.
{
    uint8_t i;
    for (i = 0u; i < whitelist_count; i++)
    {
        if (strcmp(numero, whitelist[i]) == 0)
            return (int)i;
    }
    return -1;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void neighbourhood_init(void)
{
    uint8_t i;
    whitelist_count = 0u;
    for (i = 0u; i < CALLERS_SEED_COUNT && whitelist_count < NEIGHBOURHOOD_WHITELIST_MAX; i++)
    {
        /* la seed se asume válida  */
        strncpy(whitelist[whitelist_count], callers_seed[i], NEIGHBOURHOOD_PHONE_LEN_MAX);
        whitelist[whitelist_count][NEIGHBOURHOOD_PHONE_LEN_MAX] = '\0';
        whitelist_count++;
    }
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas: Whitelist (consumido por gsm.c y ble.c):

bool neighbourhood_caller_authorized(const char *numero)
{
    if (numero == NULL || numero[0] == '\0')
        return false;
    return (find_in_whitelist(numero) >= 0);
}

uint8_t neighbourhood_caller_count(void)
{
    return whitelist_count;
}

const char *neighbourhood_caller_get(uint8_t idx)
{
    if (idx >= whitelist_count)
        return "";
    return whitelist[idx];
}

uint8_t neighbourhood_caller_add(const char *numero)
{
    if (!is_valid_phone(numero))
        return 3u; /* inválido     */
    if (find_in_whitelist(numero) >= 0)
        return 1u; /* ya existe    */
    if (whitelist_count >= NEIGHBOURHOOD_WHITELIST_MAX)
        return 2u; /* lista llena  */

    strncpy(whitelist[whitelist_count], numero, NEIGHBOURHOOD_PHONE_LEN_MAX);
    whitelist[whitelist_count][NEIGHBOURHOOD_PHONE_LEN_MAX] = '\0';
    whitelist_count++;
    put_event_task_eeprom(EV_EEPROM_PERSIST_WHITELIST); /* persistir  a EEPROM */
    return 0u;
}

uint8_t neighbourhood_caller_remove(const char *numero)
{
    int idx;
    uint8_t i;

    if (!is_valid_phone(numero))
        return 2u;
    idx = find_in_whitelist(numero);
    if (idx < 0)
        return 1u;

    /* compactar el hueco hacia abajo : preserva orden */
    for (i = (uint8_t)idx; i + 1u < whitelist_count; i++)
    {
        strcpy(whitelist[i], whitelist[i + 1u]);
    }
    whitelist_count--;
    whitelist[whitelist_count][0] = '\0';
    put_event_task_eeprom(EV_EEPROM_PERSIST_WHITELIST); /* persistir a EEPROM */
    return 0u;
}

/* -------------------------------------------------------------------------------------*/
// APIs internas usadas SOLO por eeprom.c al cargar la whitelist desde EEPROM.
// Saltean la validación de is_valid_phone y NO encolan PERSIST (evita loop).

void neighbourhood_whitelist_clear(void)
{
    uint8_t i;
    whitelist_count = 0u;
    for (i = 0u; i < NEIGHBOURHOOD_WHITELIST_MAX; i++)
    {
        whitelist[i][0] = '\0';
    }
}

void neighbourhood_whitelist_append_unchecked(const char *numero)
{
    if (numero == NULL)
        return;
    if (whitelist_count >= NEIGHBOURHOOD_WHITELIST_MAX)
        return;
    if (numero[0] == '\0')
        return; /* descartar entries vacíos en la EEPROM */

    strncpy(whitelist[whitelist_count], numero, NEIGHBOURHOOD_PHONE_LEN_MAX);
    whitelist[whitelist_count][NEIGHBOURHOOD_PHONE_LEN_MAX] = '\0';
    whitelist_count++;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas :SMS:

uint8_t neighbourhood_sms_count(void)
{
    return SMS_COUNT;
}

const char *neighbourhood_sms_recipient(uint8_t idx)
{
    if (idx < SMS_COUNT)
        return sms_recipients[idx];
    return "";
}

const char *neighbourhood_sms_message(void)
{
    return SMS_MESSAGE;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas : Credenciales BLE:

const char *neighbourhood_ble_username(void) { return BLE_USERNAME; }
const char *neighbourhood_ble_password(void) { return BLE_PASSWORD; }
