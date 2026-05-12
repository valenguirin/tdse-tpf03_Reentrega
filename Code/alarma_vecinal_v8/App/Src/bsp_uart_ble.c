/*
 * BSP de la UART del BLE (USART1 → HM-10 / BT05).
 * Único .c que toca huart1 y los handles DMA hdma_usart1_*.
 * Para portar a otro chip BLE: se reemplaza sólo este archivo.
 *
 * TX por DMA1 Canal 4 (Normal, Mem→Periph).
 * RX por DMA1 Canal 5 (Circular). Sin UART IDLE: el HM-10 fragmenta los
 * comandos en paquetes BLE separados por silencios y dispararía IDLEs
 * espurios. El parser polea CNDTR y emite línea al ver \r/\n o tras
 * LINE_IDLE_MS sin bytes nuevos.
 */

/* -------------------------------------------------------------------------------------*/
#include "bsp_uart_ble.h"
#include "usart.h" //handle huart1 generado por CubeMX
#include "stm32f1xx_hal.h"
#include <string.h>

/* CubeMX genera hdma_usart1_rx en usart.c pero no lo expone en usart.h.
 * Hace falta para leer CNDTR del DMA RX → extern directo. */
extern DMA_HandleTypeDef hdma_usart1_rx;

/* -------------------------------------------------------------------------------------*/
// Aliases del config para no propagar BLE_LINE_MAX por todo el archivo:

#define RX_BUF_SIZE BLE_RX_BUF_SIZE
#define LINE_IDLE_MS BLE_LINE_IDLE_MS

/* -------------------------------------------------------------------------------------*/
// Estado interno:

static volatile uint8_t rx_buf[RX_BUF_SIZE]; /* DMA escribe aquí continuamente   */
static uint16_t rx_tail;                     /* siguiente byte que toca leer     */

static char line_ready_buf[BSP_UART_BLE_LINE_MAX + 1u]; /* slot estable hasta line_consume()*/
static char line_pending[BSP_UART_BLE_LINE_MAX + 1u];   /* acumula mientras no hay terminator*/
static uint8_t line_pending_len;
static uint8_t line_pending_overflow; /* 1 = línea > LINE_MAX, descartar  */
static uint32_t last_byte_ms;         /* HAL_GetTick() del último byte    */

volatile uint8_t bsp_uart_ble_tx_busy_flag = 0u;
volatile uint8_t bsp_uart_ble_line_ready_flag = 0u;

/* -------------------------------------------------------------------------------------*/
// Helpers privados del parser:

// Pasa line_pending al slot estable y levanta el flag.
// Trim de espacios y \r\n en ambos extremos: las apps móviles BLE
// suelen mandar "Admin " con un espacio extra y eso rompía el match.
static void emit_pending_as_line(void)
{
    uint8_t start = 0u;

    if (line_pending_len == 0u)
        return;

    /* trim trailing */
    while (line_pending_len > 0u &&
           (line_pending[line_pending_len - 1u] == ' ' ||
            line_pending[line_pending_len - 1u] == '\t' ||
            line_pending[line_pending_len - 1u] == '\r' ||
            line_pending[line_pending_len - 1u] == '\n'))
    {
        line_pending_len--;
    }

    /* trim leading */
    while (start < line_pending_len &&
           (line_pending[start] == ' ' || line_pending[start] == '\t'))
    {
        start++;
    }

    if (line_pending_len == start)
    {
        line_pending_len = 0u; /* todo era whitespace, descartar */
        return;
    }

    memcpy(line_ready_buf, line_pending + start, line_pending_len - start);
    line_ready_buf[line_pending_len - start] = '\0';
    line_pending_len = 0u;
    bsp_uart_ble_line_ready_flag = 1u;
}

// Consume los bytes nuevos del DMA RX y arma una línea cuando ve un
// terminator o cuando pasaron LINE_IDLE_MS sin bytes nuevos. Si line_ready
// ya está en 1, no toca nada: el slot queda ocupado hasta line_consume().
static void parser_pump(void)
{
    uint16_t head;
    uint16_t avail;
    uint16_t i;
    uint8_t c;
    uint32_t now_ms;

    if (bsp_uart_ble_line_ready_flag)
    {
        return; /* slot ocupado */
    }

    /* head = posición donde el DMA va a escribir el próximo byte.
     * CNDTR cuenta bytes que faltan hasta el final, por lo que head = SIZE - CNDTR. */
    head = (uint16_t)(RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
    now_ms = HAL_GetTick();

    if (head != rx_tail)
    {
        last_byte_ms = now_ms; /*si  hubo bytes entonces reset del idle */

        /* cantidad de bytes nuevos contemplando el wrap-around */
        avail = (head >= rx_tail) ? (uint16_t)(head - rx_tail)
                                  : (uint16_t)(RX_BUF_SIZE - rx_tail + head);

        for (i = 0u; i < avail; i++)
        {
            c = rx_buf[rx_tail];
            rx_tail = (uint16_t)((rx_tail + 1u) % RX_BUF_SIZE);

            if (c == '\r' || c == '\n')
            {
                if (line_pending_overflow)
                {
                    line_pending_overflow = 0u; /* descartar resto y resetear */
                    line_pending_len = 0u;
                    continue;
                }
                if (line_pending_len == 0u)
                {
                    continue; /* terminator suelto, ignorar */
                }
                emit_pending_as_line();
                return;
            }

            if (line_pending_overflow)
                continue;

            if (line_pending_len < BSP_UART_BLE_LINE_MAX)
            {
                line_pending[line_pending_len++] = (char)c;
            }
            else
            {
                line_pending_overflow = 1u; /* desborde defensivo */
                line_pending_len = 0u;
            }
        }
    }

    /* idle timeout para apps móviles que no mandan terminator */
    if (line_pending_len > 0u && (now_ms - last_byte_ms) >= LINE_IDLE_MS)
    {
        emit_pending_as_line();
    }
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void bsp_uart_ble_init(void)
{
    bsp_uart_ble_tx_busy_flag = 0u;
    bsp_uart_ble_line_ready_flag = 0u;
    rx_tail = 0u;
    line_pending_len = 0u;
    line_pending_overflow = 0u;
    line_ready_buf[0] = '\0';
    last_byte_ms = HAL_GetTick();

    HAL_UART_Receive_DMA(&huart1, (uint8_t *)rx_buf, RX_BUF_SIZE);
}

/* -------------------------------------------------------------------------------------*/

void bsp_uart_ble_tx(const uint8_t *buf, uint16_t len)
{
    bsp_uart_ble_tx_busy_flag = 1u;
    HAL_UART_Transmit_DMA(&huart1, (uint8_t *)buf, len);
}

/* -------------------------------------------------------------------------------------*/

const char *bsp_uart_ble_line_get(void)
{
    parser_pump();
    return line_ready_buf;
}

void bsp_uart_ble_line_consume(void)
{
    /* sólo libera el slot. El parser avanza recién en el próximo line_get(). */
    bsp_uart_ble_line_ready_flag = 0u;
    line_ready_buf[0] = '\0';
}

/* -------------------------------------------------------------------------------------*/
// AT commands y URCs del HM-10. Los matchers de URC ignoran mayúsculas
// porque algunos firmware emiten "OK+SET" o "OK+Set" según versión.

static const char AT_NOTI1[] = "AT+NOTI1\r\n";
static const char AT_DISC[] = "AT";

const uint8_t *bsp_uart_ble_at_noti1(uint16_t *len)
{
    *len = (uint16_t)(sizeof(AT_NOTI1) - 1u);
    return (const uint8_t *)AT_NOTI1;
}

const uint8_t *bsp_uart_ble_at_disc(uint16_t *len)
{
    *len = (uint16_t)(sizeof(AT_DISC) - 1u);
    return (const uint8_t *)AT_DISC;
}

// compara prefix con line[0..] ignorando mayúsculas. Helper local del BSP.
static uint8_t starts_with_ci(const char *line, const char *prefix)
{
    uint8_t i = 0u;
    char a, b;
    while (prefix[i] != '\0')
    {
        a = line[i];
        if (a == '\0')
            return 0u;
        b = prefix[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z')
            b = (char)(b + ('a' - 'A'));
        if (a != b)
            return 0u;
        i++;
    }
    return 1u;
}

uint8_t bsp_uart_ble_is_urc_connect(const char *line)
{
    return starts_with_ci(line, "OK+CONN");
}

uint8_t bsp_uart_ble_is_urc_disconnect(const char *line)
{
    return starts_with_ci(line, "OK+LOST");
}

// Acepta "OK+Set" (respuesta a AT+NOTI1) o "OK" pelado (firmware viejo).
// El caller tiene que chequear OK+CONN/OK+LOST primero, el orden importa.
uint8_t bsp_uart_ble_is_setup_ack(const char *line)
{
    return (uint8_t)(starts_with_ci(line, "OK+Set") || starts_with_ci(line, "OK"));
}

/* -------------------------------------------------------------------------------------*/
// ISR handlers. Los invoca bsp_uart_callbacks.c.

void bsp_uart_ble_isr_tx_complete(void)
{
    bsp_uart_ble_tx_busy_flag = 0u;
}

void bsp_uart_ble_isr_error(void)
{
    /* re-arma DMA y resetea tail para no procesar bytes a medias */
    HAL_UART_Receive_DMA(&huart1, (uint8_t *)rx_buf, RX_BUF_SIZE);
    rx_tail = 0u;
    line_pending_len = 0u;
    line_pending_overflow = 0u;
}
