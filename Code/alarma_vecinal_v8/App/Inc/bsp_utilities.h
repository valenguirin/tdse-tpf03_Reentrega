/*
 * bsp_utilities.h : utilidades de plataforma y región
 *
 * Funciones auxiliares que son específicas del hardware de despliegue
 * (país, operador, formato de datos) y no pertenecen a un periférico concreto.
 *
 * Para portar a otra región: cambiar las constantes en bsp_utilities.c.
 * El resto del código (gsm.c, ble.c, etc.) llama estas funciones sin
 * conocer el formato regional.
 */

#ifndef BSP_UTILITIES_H
#define BSP_UTILITIES_H

/* ────────────────────────────────────────────────────────────────────────── */
// Normalización de números telefónicos
/*
 * bsp_number_normalize : quitar prefijo de país del número llamante
 *
 * Los módulos GSM y BLE pueden devolver números en formato internacional
 * ("+54XXXXXXXX") o local ("XXXXXXXX") según el operador.
 * Esta función los deja siempre en formato local para comparar con la whitelist.
 * Si el número ya está en formato local, no hace nada.
 */
void bsp_utilities_number_normalize(char *numero);

#endif /* BSP_UTILITIES_H */
