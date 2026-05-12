/*
 * Autenticación de doble factor para la sesión BLE: usuario + contraseña.
 * Funciones puras, portables (sin dependencias del proyecto).
 *
 * Las credenciales reales viven en config_neighbourhood.c y se acceden
 * con neighbourhood_ble_username() / neighbourhood_ble_password().
 *
 * Lo usa ble.c en los estados ST_BLE_WAIT_USER y ST_BLE_WAIT_PASS.
 */

#ifndef AUTH_UTILS_H
#define AUTH_UTILS_H

#include <stdint.h>
#include <stdbool.h>

bool auth_user_match(const char *input, const char *expected);   /* ignora mayúsculas    */
bool auth_pass_match(const char *input, const char *expected);   /* respeta mayúsculas   */

#endif
