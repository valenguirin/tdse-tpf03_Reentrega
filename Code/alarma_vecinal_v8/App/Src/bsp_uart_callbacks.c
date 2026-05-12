/*
 * Despachador único de callbacks HAL para las UARTs del presente trabajo.
 * Los símbolos HAL_UART_* son __weak: sólo puede haber una definición
 * "strong" en todo el binario. Acá viven y el dispatch va por
 * huart->Instance a la ISR del BSP que corresponde.
 *
 * Cero lógica de dominio, sólo ruteo.
 */

/* -------------------------------------------------------------------------------------*/
#include "bsp_uart_gsm.h"
#include "bsp_uart_ble.h"
#include "usart.h"

/* -------------------------------------------------------------------------------------*/

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        bsp_uart_gsm_isr_tx_complete();
    } /* GSM */
    else if (huart->Instance == USART1)
    {
        bsp_uart_ble_isr_tx_complete();
    } /* BLE */
}

/* -------------------------------------------------------------------------------------*/

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        bsp_uart_gsm_isr_error();
    } /* GSM */
    else if (huart->Instance == USART1)
    {
        bsp_uart_ble_isr_error();
    } /* BLE */
}

/* -------------------------------------------------------------------------------------*/

// Sólo lo dispara el GSM (ReceiveToIdle_DMA). El BLE usa Receive_DMA circular sin IDLE.
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        bsp_uart_gsm_isr_rx_event(Size);
    }
}
