/*
 * Configuración del GSM: timings, tamaños de buffers y prefijo de país.
 * Tocar acá si cambia la operadora, el país, o si se necesitan SMS más
 * largos o más buffer de RX.
 *
 * No cambia la lógica de la FSM ni del BSP, sólo valores numéricos.
 * Tiempos en ms (1 tick = 1 ms). Tamaños en bytes.
 */

#ifndef GSM_CONFIG_H
#define GSM_CONFIG_H

/* -------------------------------------------------------------------------------------*/
// Timings de la FSM (ms):

#define GSM_CMD_OK_TIMEOUT_MS         1000u   /* espera respuesta a AT+CLIP / AT+CMGF / AT+CREG  */
#define GSM_CMGS_PROMPT_TIMEOUT_MS    3000u   /* espera prompt '>' tras AT+CMGS                  */
#define GSM_SMS_TEXT_TIMEOUT_MS      30000u   /* espera "+CMGS:" tras enviar el texto + Ctrl+Z   */
#define GSM_IDLE_NET_TICK_MS          5000u   /* polling de re-CREG en IDLE_NET                  */
#define GSM_WAITING_CLIP_MS           5000u   /* espera +CLIP: tras detectar RING                */
#define GSM_HANGUP_WAIT_MS             500u   /* espera OK del ATH en CALL_HANGUP_WAIT           */
#define GSM_POST_ATH_COOLDOWN_MS       500u   /* guard para que el SIM800L libere radio (Bug 1.5)*/

/* -------------------------------------------------------------------------------------*/
// Buffers y límites:

#define GSM_RX_BUF_SIZE                 64u   /* mayor que la respuesta más larga esperada (+CREG + eco ~30 B) */
#define GSM_CALLER_NUMBER_LEN_MAX       20u   /* máximo del número del llamante extraído de +CLIP              */
#define GSM_CMGS_BUF_SIZE               48u   /* AT+CMGS="+54XXXXXXXXXX"\r\n cabe holgado                      */
#define GSM_SMS_TEXT_BUF_SIZE          162u   /* SMS GSM-7 máx 160 chars + Ctrl+Z + '\0'                       */

/* -------------------------------------------------------------------------------------*/
// Configuración del sitio:

#define GSM_COUNTRY_PREFIX            "+54"   /* prefijo internacional, cambiar para otro país */

#endif
