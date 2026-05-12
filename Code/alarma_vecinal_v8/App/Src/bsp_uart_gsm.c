/*
 * BSP de la UART del GSM (USART3 es SIM800L).
 * Único .c que toca huart3 y los handles hdma_usart3_*.
 *
 * TX por DMA1 Canal 2. HAL_UART_Transmit_DMA vuelve
 * enseguida; el TC dispara HAL_UART_TxCpltCallback.
 *
 * RX por DMA1 Canal 3 (Normal) + UART IDLE IRQ. El DMA se detiene tras
 * cada burst y rx_buf queda estable durante varios ticks para gsm.c.
 */

/* -------------------------------------------------------------------------------------*/
#include "bsp_uart_gsm.h"
#include "gsm_config.h" //tamaños y timings centralizados
#include "usart.h"      //huart3 + handles DMA generados por CubeMX
#include <string.h>

/* -------------------------------------------------------------------------------------*/
#define RX_BUF_SIZE GSM_RX_BUF_SIZE

/* -------------------------------------------------------------------------------------*/

static volatile uint8_t rx_buf[RX_BUF_SIZE];        /* destino del DMA RX        */
volatile uint8_t bsp_uart_gsm_burst_ready_flag = 0; /* 1 = hay burst listo       */
volatile uint8_t bsp_uart_gsm_burst_size_val = 0;   /* bytes del burst           */
volatile uint8_t bsp_uart_gsm_tx_busy_flag = 0;     /* 1 = DMA TX en curso       */

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void bsp_uart_gsm_init(void)
{
    bsp_uart_gsm_burst_ready_flag = 0;
    bsp_uart_gsm_burst_size_val = 0;
    bsp_uart_gsm_tx_busy_flag = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, (uint8_t *)rx_buf, RX_BUF_SIZE);
}

/* -------------------------------------------------------------------------------------*/

void bsp_uart_gsm_tx(const uint8_t *buf, uint16_t len)
{
    bsp_uart_gsm_tx_busy_flag = 1u;
    HAL_UART_Transmit_DMA(&huart3, (uint8_t *)buf, len);
}

/* -------------------------------------------------------------------------------------*/

uint8_t bsp_uart_gsm_burst_byte(uint8_t idx)
{
    return rx_buf[idx]; /* DMA detenido indica lectura segura */
}

const uint8_t *bsp_uart_gsm_burst_ptr(void)
{
    return (const uint8_t *)rx_buf; /* puntero estable hasta consume */
}

void bsp_uart_gsm_burst_consume(void)
{
    bsp_uart_gsm_burst_ready_flag = 0u;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, (uint8_t *)rx_buf, RX_BUF_SIZE);
}

void bsp_uart_gsm_rx_flush(void)
{
    bsp_uart_gsm_burst_ready_flag = 0u;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, (uint8_t *)rx_buf, RX_BUF_SIZE);
}

/* -------------------------------------------------------------------------------------*/
// ISR handlers. Los invoca bsp_uart_callbacks.c.

void bsp_uart_gsm_isr_rx_event(uint16_t Size)
{
    bsp_uart_gsm_burst_size_val = (uint8_t)Size;
    bsp_uart_gsm_burst_ready_flag = 1u;
}

// Si el SIM800L se apaga, su TX queda en LOW , eso da framing errors  y HAL aborta DMA.
// Re-arma sólo si no hay burst pendiente, así evita pisarlo.
void bsp_uart_gsm_isr_error(void)
{
    if (!bsp_uart_gsm_burst_ready_flag)
    {
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, (uint8_t *)rx_buf, RX_BUF_SIZE);
    }
}

void bsp_uart_gsm_isr_tx_complete(void)
{
    bsp_uart_gsm_tx_busy_flag = 0u;
}
