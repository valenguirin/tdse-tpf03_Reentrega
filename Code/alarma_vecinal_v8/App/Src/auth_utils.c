/*
 * Comparación de credenciales para el login BLE.
 * Dos funciones espejadas: una ignora mayúsculas (usuario), la otra no (contraseña).

 * Las credenciales reales viven en config_neighbourhood.c y se piden con
 * neighbourhood_ble_username() / neighbourhood_ble_password().
 */

/* -------------------------------------------------------------------------------------*/
#include "auth_utils.h"
#include <string.h> //para strlen

/* -------------------------------------------------------------------------------------*/
// Helper privado para la comparación de usuario (ignora mayúsculas)

// Pasa A..Z a a..z. No usa tolower() de ctype.h.
static char to_lower_ascii(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas

bool auth_user_match(const char *input, const char *expected)
{
    if (input == NULL || expected == NULL)
        return false; /* defensa contra null  */

    size_t len_in = strlen(input);
    size_t len_exp = strlen(expected);
    if (len_in != len_exp)
        return false; /* atajo por longitud   */

    /* compara byte a byte en minúscula: "Admin", "ADMIN" y "admin"
       son el mismo usuario. */
    for (size_t i = 0; i < len_in; i++)
    {
        if (to_lower_ascii(input[i]) != to_lower_ascii(expected[i]))
            return false;
    }
    return true;
}

bool auth_pass_match(const char *input, const char *expected)
{
    if (input == NULL || expected == NULL)
        return false; /* defensa contra null  */

    size_t len_in = strlen(input);
    size_t len_exp = strlen(expected);
    if (len_in != len_exp)
        return false; /* atajo por longitud   */

    /* La contraseña distingue mayúsculas: comparación tal cual. */
    for (size_t i = 0; i < len_in; i++)
    {
        if (input[i] != expected[i])
            return false;
    }
    return true;
}
