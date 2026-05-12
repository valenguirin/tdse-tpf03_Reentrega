/*
 * FSM del módulo BLE (HM-10) con 6 estados.
 * Login doble factor, sesión con comandos
 *  ADD/DEL/LIST/OUT y cierre forzado por activación de alarma.

 * Manda al sistema: EV_SYS_BLE_USER_CONNECTED / EV_SYS_BLE_USER_AUTHED /
 * EV_SYS_BLE_USER_DISCONNECTED (ver task_system_interface.h).
 * Consume del sistema: EV_BLE_FORCE_CLOSE (ver ble_interface.h).


 */

/* -------------------------------------------------------------------------------------*/
#include "ble.h"
#include "ble_config.h"               //tiempos y sizes
#include "bsp_uart_ble.h"             //TX/RX del HM-10 + AT
#include "auth_utils.h"               //match user/pass
#include "config_neighbourhood.h"     //credenciales BLE + whitelist editable
#include "task_system_interface.h"    //eventos hacia el sistema
#include "ble_interface.h"            //órdenes del sistema hacia el BLE
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------------------*/
// IDs internos de comandos AT al HM-10. El string real lo arma el BSP.

#define CMD_NOTI1   1u    /* AT+NOTI1 : pide URC OK+CONN/OK+LOST */

/* -------------------------------------------------------------------------------------*/
// Respuestas predefinidas hacia el peer. Esto va incluido para
// que el cliente sepa en qué fase del login está sin tener que recordarlo.

static const char RSP_HELLO[]    = "HELLO. USER?\r\n";
static const char RSP_USER_OK[]  = "USER OK. PASS?\r\n";
static const char RSP_AUTH_OK[]  = "AUTH OK. CMDS: ADD/DEL/LIST/OUT\r\n";
static const char RSP_OK[]       = "OK\r\n";
static const char RSP_BYE[]      = "BYE\r\n";
static const char RSP_ERR_AUTH[] = "ERROR AUTH. USER?\r\n";
static const char RSP_ERR_CMD[]  = "ERROR CMD\r\n";
static const char RSP_ERR_NUM[]  = "ERROR NUM\r\n";
static const char RSP_ERR_DUP[]  = "ERROR DUP\r\n";
static const char RSP_ERR_FULL[] = "ERROR FULL\r\n";
static const char RSP_NOT_FND[]  = "NOT FOUND\r\n";

/* -------------------------------------------------------------------------------------*/
//Implementación privada de la FSM del BLE:

typedef enum {
    ST_BLE_SETUP_NOTI_WAITING ,   /* AT+NOTI1 enviado; espera "OK+Set" o asume HM-10 ya configurado */
    ST_BLE_IDLE_DISCONNECTED  ,   /* sin peer; espera "OK+CONN" o primera línea del peer            */
    ST_BLE_WAIT_USER          ,   /* peer conectado; espera nombre de usuario                       */
    ST_BLE_WAIT_PASS          ,   /* user OK; espera password                                       */
    ST_BLE_SESSION_ACTIVE     ,   /* autenticado; acepta ADD/DEL/LIST/OUT                           */
    ST_BLE_DISCONNECTING      ,   /* cierre de sesión; espera tick para que el BYE salga por DMA    */
} FSM_STATUS_BLE;

static FSM_STATUS_BLE fsm_ble  = ST_BLE_SETUP_NOTI_WAITING;
static uint32_t       tick_ble = 0;

/* TX aplazado. Si tx_busy=1 al momento de enviar, el envío queda guardado
 * acá y el próximo update lo reintenta. Sin pérdidas silenciosas. */
static uint8_t        tx_pending_at;        /* id de AT pendiente (0 = nada)          */
static const uint8_t *tx_pending_rsp;       /* respuesta de texto pendiente (NULL=na) */
static uint16_t       tx_pending_rsp_len;

/* serializa la respuesta de LIST. Tamaño en ble_config.h */
static char list_buf[BLE_LIST_BUF_SIZE];

/* -------------------------------------------------------------------------------------*/
// Helpers privados de TX hacia el HM-10:

// Si el DMA TX está ocupado, guarda el envío como pendiente y vuelve.
static void op_bsp_uart_ble_tx(const uint8_t *buf, uint16_t len, uint8_t cmd_id)
{
    if (bsp_uart_ble_tx_busy()) {
        if (cmd_id != 0u) {
            tx_pending_at = cmd_id;
        } else {
            tx_pending_rsp     = buf;
            tx_pending_rsp_len = len;
        }
        return;
    }
    bsp_uart_ble_tx(buf, len);
}

// Despacha un AT por id. El string real lo arma el BSP.
static void send_at(uint8_t cmd_id)
{
    const uint8_t *buf = NULL;
    uint16_t       len = 0u;

    switch (cmd_id) {
        case CMD_NOTI1: buf = bsp_uart_ble_at_noti1(&len); break;
        default:        return;
    }
    op_bsp_uart_ble_tx(buf, len, cmd_id);
}

// Manda una respuesta de texto al peer.
static void send_rsp(const char *rsp)
{
    op_bsp_uart_ble_tx((const uint8_t *)rsp, (uint16_t)strlen(rsp), 0u);
}

// Reintenta el TX aplazado. AT antes que respuesta: la sesión BLE no
// avanza sin el OK del HM-10.
static void flush_pending_tx(void)
{
    if (bsp_uart_ble_tx_busy()) return;

    if (tx_pending_at != 0u) {
        uint8_t c     = tx_pending_at;
        tx_pending_at = 0u;
        send_at(c);
    }
    else if (tx_pending_rsp != NULL) {
        const uint8_t *p   = tx_pending_rsp;
        uint16_t       l   = tx_pending_rsp_len;
        tx_pending_rsp     = NULL;
        tx_pending_rsp_len = 0u;
        bsp_uart_ble_tx(p, l);
    }
}

/* -------------------------------------------------------------------------------------*/
// Helpers privados de parsing locales a ble.c:

// sin distinguir mayúsculas.
static uint8_t starts_with_ci(const char *line, const char *prefix)
{
    uint8_t i = 0u;
    char a, b;
    while (prefix[i] != '\0') {
        a = line[i];
        if (a == '\0') return 0u;
        b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
        if (a != b) return 0u;
        i++;
    }
    return 1u;
}

// Salta el verbo y devuelve el puntero al primer carácter del argumento.
// Si no hay argumento, devuelve el puntero al '\0' final.
static const char *skip_verb(const char *line)
{
    while (*line != '\0' && *line != ' ' && *line != '\t') line++;
    while (*line == ' '  || *line == '\t')                 line++;
    return line;
}

/* -------------------------------------------------------------------------------------*/
// Arma la respuesta a LIST en list_buf: "1133445566, 1199887766, ...\r\n".

static void build_list_response(void)
{
    uint8_t  i, n;
    uint16_t off = 0u;
    const char *num;
    uint8_t  nlen;

    n = neighbourhood_caller_count();
    if (n == 0u) {
        const char empty[] = "(empty)\r\n";
        memcpy(list_buf, empty, sizeof(empty));
        return;
    }

    for (i = 0u; i < n; i++) {
        num  = neighbourhood_caller_get(i);
        nlen = (uint8_t)strlen(num);
        if (off + nlen + 4u >= sizeof(list_buf)) break;   /* defensivo */
        memcpy(list_buf + off, num, nlen);  off += nlen;
        if (i + 1u < n) {
            list_buf[off++] = ',';
            list_buf[off++] = ' ';
        }
    }
    list_buf[off++] = '\r';
    list_buf[off++] = '\n';
    list_buf[off]   = '\0';
}

/* -------------------------------------------------------------------------------------*/
//  respuesta para ADD y DEL.  cada entrada es el
// string. Si cambian los códigos en config_neighbourhood, se actualiza la
// tabla .

static const char *const ADD_RSP_BY_RC[] = {
    RSP_OK,        /* 0 = OK              */
    RSP_ERR_DUP,   /* 1 = ya existe       */
    RSP_ERR_FULL,  /* 2 = lista llena     */
    RSP_ERR_NUM,   /* 3 = número inválido */
};
#define ADD_RSP_COUNT  ((uint8_t)(sizeof(ADD_RSP_BY_RC) / sizeof(ADD_RSP_BY_RC[0])))

static const char *const DEL_RSP_BY_RC[] = {
    RSP_OK,        /* 0 = OK              */
    RSP_NOT_FND,   /* 1 = no encontrado   */
    RSP_ERR_NUM,   /* 2 = número inválido */
};
#define DEL_RSP_COUNT  ((uint8_t)(sizeof(DEL_RSP_BY_RC) / sizeof(DEL_RSP_BY_RC[0])))

static const char *map_rc(const char *const table[], uint8_t count, uint8_t rc)
{
    return (rc < count) ? table[rc] : RSP_ERR_CMD;     /* defensivo */
}

/* -------------------------------------------------------------------------------------*/
// Procesa un comando dentro de SESSION_ACTIVE. Devuelve el estado siguiente.

static FSM_STATUS_BLE handle_session_command(const char *line)
{
    const char *arg;
    uint8_t     rc;

    if (starts_with_ci(line, "ADD")) {
        arg = skip_verb(line);
        rc  = neighbourhood_caller_add(arg);
        send_rsp(map_rc(ADD_RSP_BY_RC, ADD_RSP_COUNT, rc));
        return ST_BLE_SESSION_ACTIVE;
    }

    if (starts_with_ci(line, "DEL")) {
        arg = skip_verb(line);
        rc  = neighbourhood_caller_remove(arg);
        send_rsp(map_rc(DEL_RSP_BY_RC, DEL_RSP_COUNT, rc));
        return ST_BLE_SESSION_ACTIVE;
    }

    if (starts_with_ci(line, "LIST")) {
        build_list_response();
        send_rsp(list_buf);
        return ST_BLE_SESSION_ACTIVE;
    }

    if (starts_with_ci(line, "OUT")) {
        send_rsp(RSP_BYE);
        return ST_BLE_DISCONNECTING;
    }

    send_rsp(RSP_ERR_CMD);                  /* comando desconocido */
    return ST_BLE_SESSION_ACTIVE;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void ble_init(void)
{
    bsp_uart_ble_init();
    init_queue_event_task_ble();

    tx_pending_at      = 0u;
    tx_pending_rsp     = NULL;
    tx_pending_rsp_len = 0u;
    list_buf[0]        = '\0';

    /* Entrada al estado inicial: pide AT+NOTI1 y arranca a la espera del
     * "OK+Set". Al boot tx_busy=0 → el AT sale en este mismo tick; sino
     * queda en tx_pending_at y el próximo update lo lanza. */
    send_at(CMD_NOTI1);
    tick_ble = BLE_NOTI_SETUP_TIMEOUT_MS;
    fsm_ble  = ST_BLE_SETUP_NOTI_WAITING;
}

/* -------------------------------------------------------------------------------------*/

void ble_update(void) //FSM
{
    const char *linea     = NULL;
    uint8_t     hay_linea = 0u;
    uint8_t     consumida = 0u;

    /* Reintento del TX aplazado antes de procesar lógica nueva. */
    flush_pending_tx();

    /* EV_BLE_FORCE_CLOSE = alarma activa: corte de sesión. No se manda AT
     * al chip; el BT05 lo pasa al peer como spam visible. */
    if (any_event_task_ble()) {
        task_ble_ev_t ev = get_event_task_ble();
        if (ev == EV_BLE_FORCE_CLOSE && fsm_ble != ST_BLE_IDLE_DISCONNECTED &&
                                        fsm_ble != ST_BLE_DISCONNECTING) {
            tick_ble = BLE_DISCONNECT_TIMEOUT_MS;
            fsm_ble  = ST_BLE_DISCONNECTING;
            return;
        }
    }


    linea = bsp_uart_ble_line_get();
    if (bsp_uart_ble_line_ready()) {
        hay_linea = 1u;
    }

    switch (fsm_ble)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_BLE_SETUP_NOTI_WAITING:
        /* Una sola tanda de AT+NOTI1 al boot. Si no hay respuesta clara,
         * el código asume que el HM-10 ya tenía NOTI=1 (persiste tras reset).

         * El check de OK+CONN va PRIMERO. Su prefijo "OK" matchearía con
         * starts_with_ci("OK") y caería en el estado equivocado. */
        if (hay_linea) {
            if (bsp_uart_ble_is_urc_connect(linea)) {
                /* peer pareó durante el setup (HM-10 ya tenía NOTI=1) */
                put_event_task_system(EV_SYS_BLE_USER_CONNECTED);
                send_rsp(RSP_HELLO);
                fsm_ble = ST_BLE_WAIT_USER;
            }
            else if (bsp_uart_ble_is_urc_disconnect(linea)) {
                fsm_ble = ST_BLE_IDLE_DISCONNECTED;
            }
            else if (bsp_uart_ble_is_setup_ack(linea)) {
                fsm_ble = ST_BLE_IDLE_DISCONNECTED;     /* respuesta normal a AT+NOTI1 */
            }

            consumida = 1u;
        }
        else if (tick_ble == 0) {
            fsm_ble = ST_BLE_IDLE_DISCONNECTED;
        }
        else { tick_ble--; }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_BLE_IDLE_DISCONNECTED:
        /* Dos formas de detectar al peer:
         *  URC OK+CONN del HM-10 (sólo si tiene NOTI=1).
         *  Fallback: cualquier línea no-URC se trata como nombre de
         *   usuario directo. Sin HELLO porque el peer no lo esperaba. */
        if (hay_linea) {
            if (bsp_uart_ble_is_urc_connect(linea)) {
                put_event_task_system(EV_SYS_BLE_USER_CONNECTED);
                send_rsp(RSP_HELLO);
                fsm_ble = ST_BLE_WAIT_USER;
            }
            else if (!bsp_uart_ble_is_urc_disconnect(linea) && linea[0] != '\0') {
                /* probablemente comando del peer sin OK+CONN previo */
                put_event_task_system(EV_SYS_BLE_USER_CONNECTED);
                if (auth_user_match(linea, neighbourhood_ble_username())) {
                    send_rsp(RSP_USER_OK);
                    fsm_ble = ST_BLE_WAIT_PASS;
                } else {
                    send_rsp(RSP_ERR_AUTH);
                    fsm_ble = ST_BLE_WAIT_USER;
                }
            }
            consumida = 1u;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_BLE_WAIT_USER:
        /* Línea = nombre de usuario directo (sin prefijo "USER"). Más
         * natural para sesión interactiva: el peer recibió "USER?" y
         * responde "admin". */
        if (hay_linea) {
            if (bsp_uart_ble_is_urc_disconnect(linea)) {
                put_event_task_system(EV_SYS_BLE_USER_DISCONNECTED);
                fsm_ble = ST_BLE_IDLE_DISCONNECTED;
            }
            else if (auth_user_match(linea, neighbourhood_ble_username())) {
                send_rsp(RSP_USER_OK);
                fsm_ble = ST_BLE_WAIT_PASS;
            }
            else {
                send_rsp(RSP_ERR_AUTH);
                /* sin cambio de estado, el cliente puede reintentar */
            }
            consumida = 1u;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_BLE_WAIT_PASS:
        /* si Falla vuelve a WAIT_USER (re-login). */
        if (hay_linea) {
            if (bsp_uart_ble_is_urc_disconnect(linea)) {
                put_event_task_system(EV_SYS_BLE_USER_DISCONNECTED);
                fsm_ble = ST_BLE_IDLE_DISCONNECTED;
            }
            else if (auth_pass_match(linea, neighbourhood_ble_password())) {
                send_rsp(RSP_AUTH_OK);
                put_event_task_system(EV_SYS_BLE_USER_AUTHED);
                fsm_ble = ST_BLE_SESSION_ACTIVE;
            }
            else {
                send_rsp(RSP_ERR_AUTH);
                fsm_ble = ST_BLE_WAIT_USER;
            }
            consumida = 1u;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_BLE_SESSION_ACTIVE:
        if (hay_linea) {
            if (bsp_uart_ble_is_urc_disconnect(linea)) {
                put_event_task_system(EV_SYS_BLE_USER_DISCONNECTED);
                fsm_ble = ST_BLE_IDLE_DISCONNECTED;
            } else {
                FSM_STATUS_BLE next = handle_session_command(linea);
                if (next == ST_BLE_DISCONNECTING) {
                    /* timer al SALIR del estado origen */
                    tick_ble = BLE_DISCONNECT_TIMEOUT_MS;
                    fsm_ble  = ST_BLE_DISCONNECTING;
                } else {
                    fsm_ble = next;
                }
            }
            consumida = 1u;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_BLE_DISCONNECTING:
        /* Espera silenciosa para que el BYE salga por DMA antes del cierre.
         * Si el peer se desconecta durante la espera (caso HM-10 con
         * NOTI=1), el OK+LOST adelanta la salida. */
        if (hay_linea) {
            if (bsp_uart_ble_is_urc_disconnect(linea)) {
                put_event_task_system(EV_SYS_BLE_USER_DISCONNECTED);
                fsm_ble = ST_BLE_IDLE_DISCONNECTED;
            }
            consumida = 1u;     /* descarta cualquier otra línea */
        }
        else if (tick_ble == 0) {
            put_event_task_system(EV_SYS_BLE_USER_DISCONNECTED);
            fsm_ble = ST_BLE_IDLE_DISCONNECTED;
        }
        else { tick_ble--; }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        ble_init();
        break;
    }

    /* libera el slot si se usó, sino el parser queda parado */
    if (hay_linea && consumida) {
        bsp_uart_ble_line_consume();
    }
}
