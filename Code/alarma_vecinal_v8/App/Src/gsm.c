/*
 * FSM del módulo GSM (SIM800L) con 14 estados.
 * Configura el chip (CLIP, CMGF), polea red, atiende llamadas entrantes
 * con chequeo de whitelist, y manda SMS iterativos.
 *
 * Manda al sistema: EV_SYS_GSM_ON_NETWORK / EV_SYS_GSM_OFF_NETWORK /
 * EV_SYS_CALL_AUTHORIZED (ver task_system_interface.h).
 * Consume del sistema: EV_GSM_SEND_SMS (ver gsm_interface.h).
 *
 * I/O por DMA, nada bloqueante en update() (ver bsp_uart_gsm.c).
 */

/* -------------------------------------------------------------------------------------*/
#include "gsm.h"
#include "gsm_config.h"            //tiempos, sizes, prefijo de país
#include "bsp_uart_gsm.h"          //TX y RX del SIM800L
#include "bsp_gpio.h"              //flag ISR del RING
#include "config_neighbourhood.h"  //whitelist, destinatarios y mensaje SMS
#include "task_system_interface.h" //eventos hacia el sistema
#include "gsm_interface.h"         //órdenes del sistema hacia el GSM
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------------------*/
// Cabecera y cierre del CMGS armados con el prefijo del sitio:

#define CMGS_HEADER "AT+CMGS=\"" GSM_COUNTRY_PREFIX
#define CMGS_HEADER_LEN ((uint8_t)(sizeof(CMGS_HEADER) - 1u)) /* "AT+CMGS=\"+54" sin '\0' */
#define CMGS_TRAILER "\"\r\n"
#define CMGS_TRAILER_LEN ((uint8_t)(sizeof(CMGS_TRAILER) - 1u))

/* -------------------------------------------------------------------------------------*/
// IDs internos de comandos AT. Los strings reales se mandan en los helpers tx_*().

#define CMD_CLIP 1u     /* AT+CLIP=1        muestra el número del llamante      */
#define CMD_CMGF 2u     /* AT+CMGF=1        modo texto para mandar SMS          */
#define CMD_CREG 3u     /* AT+CREG?         ¿registrado en red?                 */
#define CMD_ATH 4u      /* ATH              corta la llamada entrante           */
#define CMD_CMGS 5u     /* AT+CMGS="<num>"  inicia envío de SMS al destinatario */
#define CMD_SMS_TEXT 6u /* <texto>\x1A      cuerpo del mensaje + Ctrl+Z (envía) */

static const char AT_CLIP[] = "AT+CLIP=1\r\n";
static const char AT_CMGF[] = "AT+CMGF=1\r\n";
static const char AT_CREG[] = "AT+CREG?\r\n";
static const char AT_ATH[] = "ATH\r\n";

/* -------------------------------------------------------------------------------------*/
// Implementación privada de la FSM del GSM:

typedef enum
{
    ST_GSM_SETUP_CLIP,         /* envía AT+CLIP=1, transita a CLIP_WAITING (transient) */
    ST_GSM_SETUP_CLIP_WAITING, /* espera OK; timeout → reintenta                       */
    ST_GSM_SETUP_CMGF,         /* envía AT+CMGF=1, transita a CMGF_WAITING (transient) */
    ST_GSM_SETUP_CMGF_WAITING, /* espera OK; timeout → reinicia setup                  */
    ST_GSM_IDLE_NO_NET,        /* envía AT+CREG?, transita a NET_WAITING (transient)   */
    ST_GSM_NET_WAITING,        /* espera +CREG:; timeout → reintenta                   */
    ST_GSM_IDLE_NET,           /* red activa; atiende RING, send_sms o re-poll         */
    ST_GSM_WAITING_CALL_CLIP,  /* espera +CLIP: con el número; timeout ,entonces cuelga        */
    ST_GSM_CALL_HANGUP_WAIT,   /* espera OK del ATH antes de chequear whitelist        */
    ST_GSM_CALL_CHECK,         /* chequea whitelist y notifica; sale el mismo tick     */
    ST_GSM_SMS_CMGS,           /* envía AT+CMGS=..., transita a CMGS_WAITING (transient)*/
    ST_GSM_SMS_CMGS_WAITING,   /* espera '>'; timeout → aborta                         */
    ST_GSM_SMS_TEXT,           /* envía texto+Ctrl+Z, transita a TEXT_WAITING (transient)*/
    ST_GSM_SMS_TEXT_WAITING,   /* espera +CMGS:; itera o cierra                        */
} FSM_STATUS_GSM;

static FSM_STATUS_GSM fsm_gsm = ST_GSM_SETUP_CLIP_WAITING;
static uint32_t tick_gsm = 0;
static char numero_llamante[GSM_CALLER_NUMBER_LEN_MAX]; /* sin prefijo de país, de +CLIP: */
static uint8_t sms_idx;                                 /* índice del destinatario SMS actual                     */
static uint8_t creg_fail_count;                         /* timeouts consecutivos de AT+CREG? sin respuesta        */
static uint8_t pending_cmd;                             /* AT aplazado por tx_busy (0 = nada)                     */
static uint16_t post_ath_cooldown;                      /* ms de gracia post-ATH antes de aceptar SMS             */

/* buffers para los comandos AT que se arman en runtime */
static char cmgs_buf[GSM_CMGS_BUF_SIZE];         /* AT+CMGS="+54XXXXXXXXXX"\r\n         */
static char sms_text_buf[GSM_SMS_TEXT_BUF_SIZE]; /* mensaje (máx 160) + Ctrl+Z + '\0'   */

/* -------------------------------------------------------------------------------------*/
// Helpers privados de parsing AT:

// 1 si el módulo está registrado (stat=1 local, stat=5 roaming).
// El SIM800L puede responder "+CREG: 0,1" (dos campos) o "+CREG: 1" (un campo).
static uint8_t parse_creg_stat(const char *buf)
{
    const char *p = strstr(buf, "+CREG:");
    const char *comma;
    char stat;

    if (!p)
        return 0u;
    p += 6; /* salta "+CREG:" */
    while (*p == ' ')
        p++; /* salta espacios */
    comma = strchr(p, ',');
    stat = comma ? *(comma + 1) : *p;
    return (stat == '1' || stat == '5') ? 1u : 0u;
}

// Saca el número del URC +CLIP: "+5411XXXXXXXX",145,,,,0
// Lo guarda en numero_llamante sin el prefijo "+54" para comparar con la whitelist.
static void extract_clip_number(const char *buf)
{
    const char *p = strstr(buf, "+CLIP:");
    uint8_t i;

    if (!p)
    {
        numero_llamante[0] = '\0';
        return;
    }
    p = strchr(p, '"');
    if (!p)
    {
        numero_llamante[0] = '\0';
        return;
    }
    p++; /* salta '"' de apertura */
    if (p[0] == '+' && p[1] == '5' && p[2] == '4')
        p += 3; /* salta "+54"           */
    i = 0u;
    while (*p && *p != '"' && i < (uint8_t)(sizeof(numero_llamante) - 1u))
    {
        numero_llamante[i++] = *p++;
    }
    numero_llamante[i] = '\0';
}

/* -------------------------------------------------------------------------------------*/
// Helpers privados de TX hacia el SIM800L. Cada helper arma su buffer (si
// corresponde) y dispara el DMA. El dispatcher op_bsp_uart_gsm_tx() resuelve
// el id y guarda el comando como pendiente si el TX anterior sigue en vuelo.
//

static void tx_clip(void)
{
    bsp_uart_gsm_tx((const uint8_t *)AT_CLIP, (uint16_t)(sizeof(AT_CLIP) - 1u));
}

static void tx_cmgf(void)
{
    bsp_uart_gsm_tx((const uint8_t *)AT_CMGF, (uint16_t)(sizeof(AT_CMGF) - 1u));
}

static void tx_creg(void)
{
    bsp_uart_gsm_tx((const uint8_t *)AT_CREG, (uint16_t)(sizeof(AT_CREG) - 1u));
}

static void tx_ath(void)
{
    bsp_uart_gsm_tx((const uint8_t *)AT_ATH, (uint16_t)(sizeof(AT_ATH) - 1u));
}

// Arma "AT+CMGS=\"+54<num>\"\r\n" en cmgs_buf y lo manda.
static void tx_cmgs(void)
{
    const char *num = neighbourhood_sms_recipient(sms_idx);
    uint8_t n = (uint8_t)strlen(num);
    memcpy(cmgs_buf, CMGS_HEADER, CMGS_HEADER_LEN);
    memcpy(cmgs_buf + CMGS_HEADER_LEN, num, n);
    memcpy(cmgs_buf + CMGS_HEADER_LEN + n, CMGS_TRAILER, CMGS_TRAILER_LEN);
    bsp_uart_gsm_tx((const uint8_t *)cmgs_buf, (uint16_t)(CMGS_HEADER_LEN + n + CMGS_TRAILER_LEN));
}

// Arma "<mensaje>\x1A" en sms_text_buf y lo manda. Ctrl+Z (0x1A) ordena el envío.
static void tx_sms_text(void)
{
    const char *msg = neighbourhood_sms_message();
    uint8_t n = (uint8_t)strlen(msg);
    memcpy(sms_text_buf, msg, n);
    sms_text_buf[n] = 0x1A;
    bsp_uart_gsm_tx((const uint8_t *)sms_text_buf, (uint16_t)(n + 1u));
}

// Dispatcher. Resuelve el id, kickea el DMA o aplaza si tx_busy. Ningún
// comando se pierde: el aplazado se reintenta al inicio del próximo
// gsm_update() y el *_WAITING actual sigue a la espera como si el envío
// hubiera sido puntual (latencia real ~1 tick).
static void op_bsp_uart_gsm_tx(uint8_t cmd)
{
    if (bsp_uart_gsm_tx_busy())
    {
        pending_cmd = cmd;
        return;
    }
    pending_cmd = 0u;

    switch (cmd)
    {
    case CMD_CLIP:
        tx_clip();
        break;
    case CMD_CMGF:
        tx_cmgf();
        break;
    case CMD_CREG:
        tx_creg();
        break;
    case CMD_ATH:
        tx_ath();
        break;
    case CMD_CMGS:
        tx_cmgs();
        break;
    case CMD_SMS_TEXT:
        tx_sms_text();
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------------------------------*/
// Helpers de operaciones de la FSM:

static uint8_t op_whitelist_check(void) /* busca numero_llamante en la lista del sitio */
{
    return neighbourhood_caller_authorized(numero_llamante) ? 1u : 0u;
}

static void op_sms_init(void) { sms_idx = 0u; } /* resetea índice antes de iterar    */
static void op_sms_advance(void) { sms_idx++; } /* avanza al siguiente destinatario  */
static uint8_t op_sms_has_next(void)
{
    return ((uint8_t)(sms_idx + 1u) < neighbourhood_sms_count()) ? 1u : 0u;
}

// Carga lazy del burst RX al buffer local.
static inline void cargar_rx_burst(uint8_t hay_burst, char *rx, uint8_t *rx_len)
{
    if (*rx_len == 0u && hay_burst)
    {
        *rx_len = bsp_uart_gsm_burst_size();
        memcpy(rx, bsp_uart_gsm_burst_ptr(), *rx_len);
        rx[*rx_len] = '\0';
    }
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void gsm_init(void)
{
    bsp_uart_gsm_init();
    init_queue_event_task_gsm();

    tick_gsm = 0;
    sms_idx = 0u;
    creg_fail_count = 0u;
    pending_cmd = 0u;
    post_ath_cooldown = 0u;
    numero_llamante[0] = '\0';

    /* Entrada al estado inicial: arranca AT+CLIP=1 y espera el OK. */
    op_bsp_uart_gsm_tx(CMD_CLIP);
    tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
    fsm_gsm = ST_GSM_SETUP_CLIP_WAITING;
}

/* -------------------------------------------------------------------------------------*/

void gsm_update(void) // FSM
{
    char rx[GSM_RX_BUF_SIZE + 1u];
    uint8_t rx_len = 0u;
    uint8_t burst_consumido = 0u;
    uint8_t hay_burst = bsp_uart_gsm_burst_ready();

    /* Reintento del TX aplazado: el *_WAITING actual sigue a la espera de
     * la respuesta como si el envío hubiera sido puntual . */
    if (pending_cmd != 0u && !bsp_uart_gsm_tx_busy())
    {
        op_bsp_uart_gsm_tx(pending_cmd);
    }

    switch (fsm_gsm)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_SETUP_CLIP_WAITING:
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, "OK"))
        {
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            /* confirmado , entonces arranca setup CMGF */
            op_bsp_uart_gsm_tx(CMD_CMGF);
            tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
            fsm_gsm = ST_GSM_SETUP_CMGF_WAITING;
        }
        else if (tick_gsm == 0)
        {
            /* timeout , entonces reintenta CLIP */
            op_bsp_uart_gsm_tx(CMD_CLIP);
            tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
            /* fsm_gsm queda en CLIP_WAITING */
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_SETUP_CMGF_WAITING:
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, "OK"))
        {
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            /* setup listo , entonces primera verificación de red */
            op_bsp_uart_gsm_tx(CMD_CREG);
            tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
            fsm_gsm = ST_GSM_NET_WAITING;
        }
        else if (tick_gsm == 0)
        {
            /* timeout , entonces reinicia setup desde CLIP */
            op_bsp_uart_gsm_tx(CMD_CLIP);
            tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
            fsm_gsm = ST_GSM_SETUP_CLIP_WAITING;
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_NET_WAITING:
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, "+CREG:"))
        {
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            creg_fail_count = 0u; /* respuesta OK , entonces resetea fail */
            if (parse_creg_stat(rx))
            {
                /* stat=1 o 5 , entonces hay red */
                put_event_task_system(EV_SYS_GSM_ON_NETWORK);
                tick_gsm = GSM_IDLE_NET_TICK_MS;
                fsm_gsm = ST_GSM_IDLE_NET;
            }
            else
            {
                /* otro stat , entonces sin red, re-pollea */
                put_event_task_system(EV_SYS_GSM_OFF_NETWORK);
                op_bsp_uart_gsm_tx(CMD_CREG);
                tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
                /* fsm_gsm queda en NET_WAITING */
            }
        }
        else if (tick_gsm == 0)
        {
            /* Timeout sin respuesta. Sólo emite OFF_NETWORK tras 3 timeouts
             * seguidos (~3 s) para no cortar la red por race transitorio del DMA. */
            if (++creg_fail_count >= 3u)
            {
                creg_fail_count = 0u;
                put_event_task_system(EV_SYS_GSM_OFF_NETWORK);
            }
            /* reintenta CREG */
            op_bsp_uart_gsm_tx(CMD_CREG);
            tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
            /* fsm_gsm queda en NET_WAITING */
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_IDLE_NET:
        // RING tiene prioridad sobre EV_GSM_SEND_SMS..

        if (post_ath_cooldown > 0u)
            post_ath_cooldown--;

        if (bsp_gpio_ring_isr_triggered())
        {
            cargar_rx_burst(hay_burst, rx, &rx_len);
            if (rx_len > 0u && strstr(rx, "+CLIP:"))
            {
                /* fast path: el burst del RING ya trae +CLIP en este mismo tick */
                extract_clip_number(rx);
                bsp_uart_gsm_burst_consume();
                burst_consumido = 1u;
                op_bsp_uart_gsm_tx(CMD_ATH);
                tick_gsm = GSM_HANGUP_WAIT_MS;
                fsm_gsm = ST_GSM_CALL_HANGUP_WAIT;
            }
            else
            {
                /* a la espera del +CLIP en próximo burst */
                tick_gsm = GSM_WAITING_CLIP_MS;
                fsm_gsm = ST_GSM_WAITING_CALL_CLIP;
            }
        }
        else if (post_ath_cooldown == 0u && any_event_task_gsm())
        {
            get_event_task_gsm(); /* consume EV_GSM_SEND_SMS */
            op_sms_init();
            /* arranca envío de SMS al primer destinatario */
            op_bsp_uart_gsm_tx(CMD_CMGS);
            tick_gsm = GSM_CMGS_PROMPT_TIMEOUT_MS;
            fsm_gsm = ST_GSM_SMS_CMGS_WAITING;
        }
        else if (tick_gsm == 0)
        {
            /* tick vencido , entonces re-verifica red */
            op_bsp_uart_gsm_tx(CMD_CREG);
            tick_gsm = GSM_CMD_OK_TIMEOUT_MS;
            fsm_gsm = ST_GSM_NET_WAITING;
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_WAITING_CALL_CLIP:
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, "+CLIP:"))
        {
            extract_clip_number(rx);
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            op_bsp_uart_gsm_tx(CMD_ATH);
            tick_gsm = GSM_HANGUP_WAIT_MS;
            fsm_gsm = ST_GSM_CALL_HANGUP_WAIT;
        }
        else if (tick_gsm == 0)
        {
            /* llamada anónima. numero_llamante se limpia para que CALL_CHECK
             * no use el número de la llamada anterior. */
            numero_llamante[0] = '\0';
            op_bsp_uart_gsm_tx(CMD_ATH); /* cuelga igual */
            tick_gsm = GSM_HANGUP_WAIT_MS;
            fsm_gsm = ST_GSM_CALL_HANGUP_WAIT;
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_CALL_HANGUP_WAIT:
        /* Espera el OK del ATH antes de chequear whitelist y disparar SMS.
         * El SIM800L tarda 100-500 ms en cortar a nivel radio. Si AT+CMGS
         * sale antes del OK, devuelve ERROR en vez de '>'. */
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, "OK"))
        {
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            fsm_gsm = ST_GSM_CALL_CHECK; /* confirmado , entonceschequea */
        }
        else if (tick_gsm == 0)
        {
            fsm_gsm = ST_GSM_CALL_CHECK; /* timeout  , entonces asume OK  */
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_CALL_CHECK:
        /* Procesamiento puro, no consulta rx, sale el mismo tick. */
        if (op_whitelist_check())
        {
            put_event_task_system(EV_SYS_CALL_AUTHORIZED); /* autorizado , entonces notifica */
        }

        numero_llamante[0] = '\0';
        (void)bsp_gpio_ring_isr_triggered();
        if (bsp_uart_gsm_burst_ready())
        {
            bsp_uart_gsm_burst_consume();
        }
        post_ath_cooldown = GSM_POST_ATH_COOLDOWN_MS;
        tick_gsm = GSM_IDLE_NET_TICK_MS;
        fsm_gsm = ST_GSM_IDLE_NET;
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_SMS_CMGS_WAITING:
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, ">"))
        {
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            /*  manda el texto */
            op_bsp_uart_gsm_tx(CMD_SMS_TEXT);
            tick_gsm = GSM_SMS_TEXT_TIMEOUT_MS;
            fsm_gsm = ST_GSM_SMS_TEXT_WAITING;
        }
        else if (tick_gsm == 0)
        {
            /* aborta SMS por timeout  */
            tick_gsm = GSM_IDLE_NET_TICK_MS;
            fsm_gsm = ST_GSM_IDLE_NET;
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_GSM_SMS_TEXT_WAITING:
        cargar_rx_burst(hay_burst, rx, &rx_len);
        if (rx_len > 0u && strstr(rx, "+CMGS:"))
        {
            bsp_uart_gsm_burst_consume();
            burst_consumido = 1u;
            if (op_sms_has_next())
            {
                /* hay más , entonces siguiente destinatario */
                op_sms_advance();
                op_bsp_uart_gsm_tx(CMD_CMGS);
                tick_gsm = GSM_CMGS_PROMPT_TIMEOUT_MS;
                fsm_gsm = ST_GSM_SMS_CMGS_WAITING;
            }
            else
            {
                /* último , entonces fin */
                tick_gsm = GSM_IDLE_NET_TICK_MS;
                fsm_gsm = ST_GSM_IDLE_NET;
            }
        }
        else if (tick_gsm == 0)
        {
            /* timeout , entonces aborta SMS */
            tick_gsm = GSM_IDLE_NET_TICK_MS;
            fsm_gsm = ST_GSM_IDLE_NET;
        }
        else
        {
            tick_gsm--;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        gsm_init();
        break;
    }

    if (hay_burst && !burst_consumido)
    {
        bsp_uart_gsm_burst_consume();
    }
}
