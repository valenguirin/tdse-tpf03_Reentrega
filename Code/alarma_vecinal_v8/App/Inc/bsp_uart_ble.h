/*
 * BSP de la UART del BLE (USART1 → HM-10).
 * TX no-bloqueante por DMA y RX por línea (parser sobre buffer DMA circular).
 *
 * TX por DMA1 Canal 4 (HAL_UART_Transmit_DMA).
 * RX por DMA1 Canal 5 modo Circular. La FSM consulta line_ready() cada
 * tick y consume con line_get() cuando hay línea completa.
 *
 * Prohibido HAL_UART_Transmit/Receive bloqueantes, rompen el tick de 1 ms.
 * Buffers, flags volátiles, callbacks y parser viven en el .c.
 */

#ifndef BSP_UART_BLE_H
#define BSP_UART_BLE_H

#include <stdint.h>
#include "ble_config.h"   /* tamaños y timings centralizados */

/* -------------------------------------------------------------------------------------*/
// alias del config para no propagar BLE_LINE_MAX por todo el código:

#define BSP_UART_BLE_LINE_MAX  BLE_LINE_MAX

/* -------------------------------------------------------------------------------------*/
// Estado interno expuesto SOLO para que los getters triviales sean static inline.
// NO escribir estas variables desde afuera de bsp_uart_ble.c.

extern volatile uint8_t bsp_uart_ble_tx_busy_flag;     /* 1 = DMA TX en curso        */
extern volatile uint8_t bsp_uart_ble_line_ready_flag;  /* 1 = hay línea para leer    */

/* -------------------------------------------------------------------------------------*/
// Init y TX:

void bsp_uart_ble_init(void);   /* arranca DMA RX circular, llamar una vez en app_init() */
void bsp_uart_ble_tx  (const uint8_t *buf, uint16_t len);

static inline uint8_t bsp_uart_ble_tx_busy(void)   /* 1 = ocupado, 0 = libre */
{
    return bsp_uart_ble_tx_busy_flag;
}

/* -------------------------------------------------------------------------------------*/

static inline uint8_t bsp_uart_ble_line_ready(void)   /* 1 = hay línea pendiente */
{
    return bsp_uart_ble_line_ready_flag;
}

const char *bsp_uart_ble_line_get    (void);   /* puntero estable hasta line_consume() */
void        bsp_uart_ble_line_consume(void);   /* libera el slot, reanuda parseo       */

/* -------------------------------------------------------------------------------------*/
// AT commands y URCs específicos del HM-10. Viven acá porque el BSP YA es
// chip-específico (RX_BUF_SIZE, LINE_IDLE_MS, modo Circular). Si se cambia
// el chip BLE, se reemplaza este BSP entero y la FSM (ble.c) sigue igual.

const uint8_t *bsp_uart_ble_at_noti1(uint16_t *len);   /* "AT+NOTI1\r\n" */
const uint8_t *bsp_uart_ble_at_disc (uint16_t *len);   /* "AT" solo    */

uint8_t bsp_uart_ble_is_urc_connect   (const char *line);   /* 1 = empieza con "OK+CONN"      */
uint8_t bsp_uart_ble_is_urc_disconnect(const char *line);   /* 1 = empieza con "OK+LOST"      */
uint8_t bsp_uart_ble_is_setup_ack     (const char *line);   /* 1 = empieza con "OK+Set" u "OK"*/

/* -------------------------------------------------------------------------------------*/
// Sólo para uso de bsp_uart_callbacks.c. No invocar desde la app.

void bsp_uart_ble_isr_tx_complete(void);
void bsp_uart_ble_isr_error      (void);

#endif
