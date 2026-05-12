/*
 * BSP de la EEPROM (24LC256 en I2C1).
 * Único .c que toca hi2c1 y hdma_i2c1_tx.
 *
 * READ : HAL_I2C_Mem_Read bloqueante. Sólo se usa en eeprom_init() antes
 *        del scheduler, donde el bloqueo es válido.
 * WRITE: HAL_I2C_Mem_Write_DMA no bloqueante. Dispara el envío por DMA1
 *        Canal 6 (I2C1_TX) y vuelve. El TC del DMA invoca
 *        HAL_I2C_MemTxCpltCallback que baja tx_done.
 *
 */

/* -------------------------------------------------------------------------------------*/
#include "bsp_eeprom.h"
#include "i2c.h" //hi2c1 generado por CubeMX

/* -------------------------------------------------------------------------------------*/
// HAL desde la ISR del DMA.

volatile uint8_t bsp_eeprom_tx_done_flag = 1u; /* libre al boot */

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void bsp_eeprom_init(void)
{
    bsp_eeprom_tx_done_flag = 1u;
    /* hi2c1 ya inicializado por MX_I2C1_Init() en main.c */
}

/* -------------------------------------------------------------------------------------*/

// Si el chip no responde (ausente, mal cableado), HAL devuelve TIMEOUT y
// la función devuelve 0, la FSM cae al fallback de "EEPROM vacía" y usa la seed.
uint8_t bsp_eeprom_read_blocking(uint16_t addr, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st;
    st = HAL_I2C_Mem_Read(&hi2c1, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, len, EEPROM_READ_TIMEOUT_MS);
    return (st == HAL_OK) ? 1u : 0u;
}

/* -------------------------------------------------------------------------------------*/

// Dispara el write DMA y vuelve. El buffer del caller tiene que seguir
// vivo hasta que tx_done()==1: el DMA usa el puntero original durante todo
// el envío.
uint8_t bsp_eeprom_write_page_kickoff(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st;

    if (!bsp_eeprom_tx_done_flag)
    {
        return 0u; /* hay un write en curso */
    }

    bsp_eeprom_tx_done_flag = 0u; /* ocupado antes del kickoff */
    st = HAL_I2C_Mem_Write_DMA(&hi2c1, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)buf, len);
    if (st != HAL_OK)
    {
        bsp_eeprom_tx_done_flag = 1u; /* falló → libera el flag */
        return 0u;
    }
    return 1u;
}

/* -------------------------------------------------------------------------------------*/
// ISR handlers. Los invocan los callbacks HAL de abajo.

void bsp_eeprom_isr_tx_complete(void)
{
    bsp_eeprom_tx_done_flag = 1u;
}

void bsp_eeprom_isr_error(void)
{
    /* cuan hay error en I2C el flag se libera y la FSM ve tx_done y
     * avanza. Si el chip no recibió el dato, el próximo boot falla el CRC
     * y se reformatea. */
    bsp_eeprom_tx_done_flag = 1u;
}

/* -------------------------------------------------------------------------------------*/
// Callbacks HAL. Definición strong única en el proyecto. Si mañana se
// suma otro device I2C en hi2c1, esto se pasa a un bsp_i2c_callbacks.c

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        bsp_eeprom_isr_tx_complete();
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        bsp_eeprom_isr_error();
    }
}
