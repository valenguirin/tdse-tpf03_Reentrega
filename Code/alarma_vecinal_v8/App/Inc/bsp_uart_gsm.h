/*
 * TX y RX no-bloqueantes por DMA.
 *
 * TX por DMA1 Canal 2 (HAL_UART_Transmit_DMA).
 * RX por DMA1 Canal 3 Normal + UART IDLE IRQ (HAL_UARTEx_ReceiveToIdle_DMA).
 * El DMA se detiene tras cada burst y el buffer queda estable hasta que
 * gsm.c llame burst_consume(). Se puede leer sin prisa durante varios ticks.
 *
 * Prohibido HAL_UART_Transmit/Receive bloqueantes, detienen la CPU ~870 µs
 * por cada 10 bytes a 115200 baud y rompen el tick.
 */

#ifndef BSP_UART_GSM_H
#define BSP_UART_GSM_H

#include <stdint.h>

/* -------------------------------------------------------------------------------------*/
// Estado interno expuesto SOLO para que los getters triviales sean static inline.
// NO escribir estas variables desde afuera de bsp_uart_gsm.c.

extern volatile uint8_t bsp_uart_gsm_burst_ready_flag;   /* 1 = hay burst listo  */
extern volatile uint8_t bsp_uart_gsm_burst_size_val;     /* bytes del burst      */
extern volatile uint8_t bsp_uart_gsm_tx_busy_flag;       /* 1 = DMA TX en curso  */

/* -------------------------------------------------------------------------------------*/
// Init y TX:

void bsp_uart_gsm_init(void);   /* arranca DMA RX, llamar una vez en app_init() */
void bsp_uart_gsm_tx  (const uint8_t *buf, uint16_t len);

static inline uint8_t bsp_uart_gsm_tx_busy(void)   /* 1 = ocupado, 0 = libre */
{
    return bsp_uart_gsm_tx_busy_flag;
}

/* -------------------------------------------------------------------------------------*/
// API de burst

static inline uint8_t bsp_uart_gsm_burst_ready(void)   /* 1 = hay burst listo */
{
    return bsp_uart_gsm_burst_ready_flag;
}

static inline uint8_t bsp_uart_gsm_burst_size(void)    /* bytes del burst pendiente */
{
    return bsp_uart_gsm_burst_size_val;
}

uint8_t        bsp_uart_gsm_burst_byte   (uint8_t idx);  /* byte en posición idx          */
const uint8_t *bsp_uart_gsm_burst_ptr    (void);         /* puntero al buffer (memcpy)    */
void           bsp_uart_gsm_burst_consume(void);         /* marca leído y re-arma DMA     */
void           bsp_uart_gsm_rx_flush     (void);         /* descarta pendiente y re-arma  */

/* -------------------------------------------------------------------------------------*/
// Sólo para uso de bsp_uart_callbacks.c. No invocar desde la app.

void bsp_uart_gsm_isr_rx_event   (uint16_t Size);
void bsp_uart_gsm_isr_error      (void);
void bsp_uart_gsm_isr_tx_complete(void);

#endif
