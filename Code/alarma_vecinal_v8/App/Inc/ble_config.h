/*
 * Configuración del BLE: timings, tamaños de buffers y límites del parser.
 * Tocar acá si el chip BLE responde más lento/rápido, si se necesitan más
 * números o líneas más largas, o si se cambia el chip por otro.
 *
 * No cambia la lógica de la FSM ni del BSP, sólo valores numéricos.
 * Tiempos en ms (1 tick = 1 ms). Tamaños en bytes.
 */

#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

/* -------------------------------------------------------------------------------------*/
// Timings de la FSM (ms):

#define BLE_NOTI_SETUP_TIMEOUT_MS    1000u   /* espera respuesta a AT+NOTI1 antes de asumir HM-10 ya configurado */
#define BLE_DISCONNECT_TIMEOUT_MS    2000u   /* espera OK+LOST tras forzar AT, si no llega asumimos disconnect */
#define BLE_DISCONNECT_GUARD_MS       200u   /* silencio en UART antes de mandar AT pelado al HM-10              */
#define BLE_IDLE_NET_TICK_MS         5000u   /* tick base de IDLE_NET (reservado para timeout de sesión)         */

/* -------------------------------------------------------------------------------------*/
// Buffers y límites del BSP:

#define BLE_RX_BUF_SIZE               128u   /* DMA circular RX, cubre ~13 ms de stream a 9600 baud sin overrun   */
#define BLE_LINE_MAX                   64u   /* longitud máxima de una línea de comando entrante                  */
#define BLE_LINE_IDLE_MS              100u   /* idle timeout para emitir línea sin '\r\n' (apps móviles BLE)      */

/* -------------------------------------------------------------------------------------*/
// Buffer para serializar la respuesta de LIST:
//   50 contactos × (13 dígitos + ", ") + "\r\n" + '\0' ≈ 760 → redondeo a 800.

#define BLE_LIST_BUF_SIZE             800u

#endif
