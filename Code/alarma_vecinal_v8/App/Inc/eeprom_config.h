/*
 * Configuración de la EEPROM: dirección I2C, page size, layout del bloque,
 * magic numbers de integridad y timeouts del BSP.
 * Tocar acá si se cambia el chip por otro modelo, si cambia el cableado
 * de A0/A1/A2, o si se aumenta NEIGHBOURHOOD_WHITELIST_MAX.
 *
 * No cambia la lógica de la FSM ni del BSP, sólo valores numéricos.
 * Tiempos en ms (1 tick = 1 ms). Tamaños en bytes.
 */

#ifndef EEPROM_CONFIG_H
#define EEPROM_CONFIG_H

/* -------------------------------------------------------------------------------------*/
// Identificación del chip (24LC256, Microchip, 32 KB, páginas de 64 B):

#define EEPROM_I2C_ADDR 0xA0u     /* 7-bit shifted (A0/A1/A2 a GND)               */
#define EEPROM_PAGE_SIZE 64u      /* tamaño de página del chip                    */
#define EEPROM_TOTAL_BYTES 32768u /* 32 KB totales (256 kbit)                     */
#define EEPROM_TWR_MS 6u          /* tiempo de write conservador (datasheet 5 ms) */

/* -------------------------------------------------------------------------------------*/
// Layout del bloque whitelist:
//   Página 0       (offset 0x0000) -> header (magic + version + count + crc16)
//   Páginas 1..13  (offsets 0x0040..) -> entries (4 entries × 16 B por página)
// Alineado a páginas para que la escritura no cruce boundaries del chip.
// La estructura del header está en eeprom.c (eeprom_header_t).

#define EEPROM_HEADER_OFFSET 0x0000u
#define EEPROM_ENTRIES_OFFSET 0x0040u
#define EEPROM_ENTRY_SIZE 16u      /* 13 dígitos + '\0' + 2 padding              */
#define EEPROM_ENTRIES_PER_PAGE 4u /* 64 B / 16 B = 4 entries por página         */

/* -------------------------------------------------------------------------------------*/
// Integridad del bloque:

#define EEPROM_MAGIC 0xA5A5C3C3u /* identifica que el bloque está formateado   */
#define EEPROM_VERSION 1u        /* incrementar si cambia el layout binario    */

/* -------------------------------------------------------------------------------------*/
// Timeout del BSP (read bloqueante en init):

#define EEPROM_READ_TIMEOUT_MS 100u

#endif
