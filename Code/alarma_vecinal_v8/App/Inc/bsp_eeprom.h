/*
 * BSP de la EEPROM (24LC256 en I2C1).
 *
 * Read bloqueante (HAL_I2C_Mem_Read), sólo se llama en eeprom_init()
 * antes del scheduler, donde el bloqueo es válido.
 * Write no-bloqueante por DMA1 Canal 6 (HAL_I2C_Mem_Write_DMA). La FSM
 * en eeprom.c espera tx_done (TC del DMA) y luego cuenta tWR antes de
 * mandar otra página.
 *
 * Prohibido HAL_I2C_*_blocking en runtime, detiene la CPU 5-10 ms.
 */

#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include <stdint.h>
#include "eeprom_config.h"   /* dirección I2C, page size, magic */

/* -------------------------------------------------------------------------------------*/
// Estado interno expuesto SOLO para que el getter trivial sea static inline.
// NO escribir esta variable desde afuera de bsp_eeprom.c.

extern volatile uint8_t bsp_eeprom_tx_done_flag;   /* 1 = libre, 0 = write en curso */

/* -------------------------------------------------------------------------------------*/
// Init y operaciones:

void    bsp_eeprom_init             (void);                                                /* una vez en app_init() */
uint8_t bsp_eeprom_read_blocking    (uint16_t addr, uint8_t *buf, uint16_t len);          /* 1 = OK, 0 = error I2C */
uint8_t bsp_eeprom_write_page_kickoff(uint16_t addr, const uint8_t *buf, uint16_t len);    /* 1 = kickeado          */

static inline uint8_t bsp_eeprom_tx_done(void)   /* 1 = libre, 0 = en curso */
{
    return bsp_eeprom_tx_done_flag;
}

/* -------------------------------------------------------------------------------------*/
// Sólo para uso del despachador HAL. No invocar desde la app.

void bsp_eeprom_isr_tx_complete(void);
void bsp_eeprom_isr_error      (void);

#endif
