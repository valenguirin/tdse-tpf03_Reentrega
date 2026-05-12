/*
 * bsp_utilities.c es para  utilidades de plataforma y región
 *
 * Para portar a otra región: cambiar COUNTRY_PREFIX y COUNTRY_PREFIX_LEN.
 */

#include "bsp_utilities.h"
#include <string.h> /* memmove, strlen */
#include <stdint.h>

/* ────────────────────────────────────────────────────────────────────────── */
// Configuración regional
/*
 * Prefijo de país a eliminar de los números telefónicos.
 * Argentina: "+54"   Colombia: "+57"   España: "+34"
 */
#define COUNTRY_PREFIX "+54"
#define COUNTRY_PREFIX_LEN 3u /* longitud de "+54" */

/* ────────────────────────────────────────────────────────────────────────── */
// Implementación
/*
 * bsp_utilities_number_normalize, se encarga de  quitar el prefijo de país COUNTRY_PREFIX
 *
 * Si numero[] empieza con "+54", desplaza el resto al inicio (in-place).
 * Ejemplo: "+541133588375"-> "1133588375"
 * Si no empieza con "+54", no modifica nada.
 */
void bsp_utilities_number_normalize(char *numero)
{
    uint8_t len = (uint8_t)strlen(numero);
    if (len > COUNTRY_PREFIX_LEN &&
        numero[0] == COUNTRY_PREFIX[0] &&
        numero[1] == COUNTRY_PREFIX[1] &&
        numero[2] == COUNTRY_PREFIX[2])
    {
        memmove(numero, numero + COUNTRY_PREFIX_LEN,
                (uint8_t)(len - COUNTRY_PREFIX_LEN + 1u));
    }
}
