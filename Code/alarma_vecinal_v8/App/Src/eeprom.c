/*
 * FSM del módulo EEPROM (24LC256) con 6 estados
 * Persiste la whitelist en EEPROM por I2C no-bloqueante.

 * eeprom_init() es bloqueante: lee EEPROM y, si el bloque es válido,
 * sobrescribe la whitelist en RAM. Si está corrupto o el chip vacío
 * encola EV_EEPROM_PERSIST_WHITELIST para grabar la seed en el primer tick.
 *
 * Consume EV_EEPROM_PERSIST_WHITELIST
 * (ver eeprom_interface.h).
 *
 * I/O por DMA en runtime, sin bloqueos en update().
 */

/* -------------------------------------------------------------------------------------*/
#include "eeprom.h"
#include "eeprom_config.h"
#include "eeprom_interface.h" //cola de eventos PERSIST_WHITELIST
#include "bsp_eeprom.h"
#include "config_neighbourhood.h" //APIs de la whitelist en RAM
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------------------*/

// El acceso es sólo por memcpy, nunca con punteros desalineados.

typedef struct __attribute__((packed))
{
    uint32_t magic;       /* identifica que el bloque está formateado */
    uint8_t version;      /* incrementar si cambia este layout */
    uint8_t count;        /* cantidad de entries válidos (0..50)   */
    uint16_t crc;         /* 8 bytes útiles + entries */
    uint8_t reserved[56]; /* padding hasta llenar la página de 64 bytes  */
} eeprom_header_t;

/* check de tamaño en compile-time: si no da 64, falla el build */
typedef char eeprom_header_size_check[(sizeof(eeprom_header_t) == 64) ? 1 : -1];

/* Páginas a escribir en cada PERSIST: 1 header + 13 de entries = 14. Las
 * 13 van siempre, aunque la lista esté corta. Así el CRC abarca un bloque
 * de tamaño fijo y la próxima lectura no ve basura. */
#define EEPROM_TOTAL_PAGES ((uint8_t)(1u + (NEIGHBOURHOOD_WHITELIST_MAX + EEPROM_ENTRIES_PER_PAGE - 1u) / EEPROM_ENTRIES_PER_PAGE))

/* -------------------------------------------------------------------------------------*/
// Implementación privada de la FSM del eeprom:

typedef enum
{
    ST_EEPROM_IDLE,         /* sin escritura; espera evento PERSIST   */
    ST_EEPROM_PREPARE_CRC,  /* calcula CRC sobre la whitelist actual  */
    ST_EEPROM_BUILD_PAGE,   /* arma la página actual en page_buf    */
    ST_EEPROM_KICKOFF_PAGE, /* lanza HAL_I2C_Mem_Write_DMA (async)  */
    ST_EEPROM_WRITING,      /* página en vuelo; espera tx_done       */
    ST_EEPROM_WAIT_TWR,     /* tx_done OK; cuenta tWR antes de la próxima */
} FSM_STATUS_EEPROM;

static FSM_STATUS_EEPROM fsm_eeprom = ST_EEPROM_IDLE;
static uint32_t tick_eeprom = 0;
static uint8_t current_page;               /* 0 = header, 1..13 = entries */
static uint16_t computed_crc;              /* snapshot al arrancar el flow */
static uint8_t page_buf[EEPROM_PAGE_SIZE]; /* buffer reusable para kickoff */

/* -------------------------------------------------------------------------------------*/
// Tabla precomputada de CRC-16 CCITT (poly 0x1021, init 0xFFFF, sin XOR final).
// Vive en RAM (512 B), se calcula al boot. Acelera el CRC de bit-a-bit a

static uint16_t crc16_table[256];

static void crc16_table_init(void)
{
    uint16_t crc, i;
    uint8_t j;
    for (i = 0u; i < 256u; i++)
    {
        crc = (uint16_t)(i << 8);
        for (j = 0u; j < 8u; j++)
        {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
        crc16_table[i] = crc;
    }
}

// Acumula CRC sobre data[]. Se puede encadenar para el header + cada entry
// sin tener que armar un buffer único de 808 bytes.
static uint16_t crc16_ccitt_continue(uint16_t crc, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0u; i < len; i++)
    {
        crc = (uint16_t)((crc << 8) ^ crc16_table[(uint8_t)((crc >> 8) ^ data[i])]);
    }
    return crc;
}

/* -------------------------------------------------------------------------------------*/
// Helpers privados de serialización:

// Llena los 16 bytes de un entry con el número null-terminated y padding 0.
// Si entry_idx >= count, queda lleno de 0 (padding limpio).
static void op_serialize_entry(uint8_t entry_idx, uint8_t *out16)
{
    const char *num;
    uint8_t nlen;

    memset(out16, 0, EEPROM_ENTRY_SIZE);
    if (entry_idx >= neighbourhood_caller_count())
        return;

    num = neighbourhood_caller_get(entry_idx);
    nlen = (uint8_t)strlen(num);
    if (nlen > NEIGHBOURHOOD_PHONE_LEN_MAX)
        nlen = NEIGHBOURHOOD_PHONE_LEN_MAX;
    memcpy(out16, num, nlen);
}

// CRC sobre los 8 bytes útiles del header + los entries reales (count × 16).
// Recorrido progresivo, sin armar el bloque entero en RAM. Sólo se ejecuta
// una vez por evento PERSIST.
static uint16_t op_compute_total_crc(void)
{
    uint8_t hdr_useful[8];
    uint8_t entry[EEPROM_ENTRY_SIZE];
    uint8_t count = neighbourhood_caller_count();
    uint16_t crc;
    uint8_t i;

    /* header sin el campo crc (queda en 0 al calcular) */
    hdr_useful[0] = (uint8_t)(EEPROM_MAGIC & 0xFFu);
    hdr_useful[1] = (uint8_t)((EEPROM_MAGIC >> 8) & 0xFFu);
    hdr_useful[2] = (uint8_t)((EEPROM_MAGIC >> 16) & 0xFFu);
    hdr_useful[3] = (uint8_t)((EEPROM_MAGIC >> 24) & 0xFFu);
    hdr_useful[4] = EEPROM_VERSION;
    hdr_useful[5] = count;
    hdr_useful[6] = 0u; /* crc placeholder */
    hdr_useful[7] = 0u;

    crc = crc16_ccitt_continue(0xFFFFu, hdr_useful, 8u);

    for (i = 0u; i < count; i++)
    {
        op_serialize_entry(i, entry);
        crc = crc16_ccitt_continue(crc, entry, EEPROM_ENTRY_SIZE);
    }
    return crc;
}

// Arma los 64 bytes de la página `page_idx` lista para el kickoff I2C.
//   page_idx == 0 : header binario.
//   page_idx >= 1 : 4 entries consecutivos (con padding 0 al final).
static void op_build_page(uint8_t page_idx, uint8_t *out64)
{
    eeprom_header_t hdr;
    uint8_t i;
    uint8_t entry_idx;

    if (page_idx == 0u)
    {
        memset(&hdr, 0xFF, sizeof(hdr)); /* reserved en valor de erase del chip */
        hdr.magic = EEPROM_MAGIC;
        hdr.version = EEPROM_VERSION;
        hdr.count = neighbourhood_caller_count();
        hdr.crc = computed_crc;
        memcpy(out64, &hdr, sizeof(hdr));
    }
    else
    {
        memset(out64, 0, EEPROM_PAGE_SIZE);
        for (i = 0u; i < EEPROM_ENTRIES_PER_PAGE; i++)
        {
            entry_idx = (uint8_t)((page_idx - 1u) * EEPROM_ENTRIES_PER_PAGE + i);
            op_serialize_entry(entry_idx, &out64[i * EEPROM_ENTRY_SIZE]);
        }
    }
}

// Dirección absoluta en EEPROM de la página dada.
static uint16_t op_page_address(uint8_t page_idx)
{
    if (page_idx == 0u)
    {
        return EEPROM_HEADER_OFFSET;
    }
    return (uint16_t)(EEPROM_ENTRIES_OFFSET + (page_idx - 1u) * EEPROM_PAGE_SIZE);
}

static void op_build_current_page(void)
{
    op_build_page(current_page, page_buf);
}

// Si el kickoff falla (chip ausente, etc.), la FSM lo detecta en WRITING
// porque tx_done queda en 1 y avanza igual. El caller asume kickoff OK.
static void op_kickoff_current_page(void)
{
    (void)bsp_eeprom_write_page_kickoff(op_page_address(current_page), page_buf, EEPROM_PAGE_SIZE);
}

/* -------------------------------------------------------------------------------------*/
// Carga inicial bloqueante (sólo se llama desde eeprom_init):

static uint8_t op_load_from_eeprom(void)
{
    eeprom_header_t hdr;
    uint8_t entries_buf[NEIGHBOURHOOD_WHITELIST_MAX * EEPROM_ENTRY_SIZE];
    uint8_t hdr_for_crc[8];
    uint16_t calc_crc;
    uint8_t i;
    char numero[NEIGHBOURHOOD_PHONE_LEN_MAX + 1u];

    /* leer header (1 página = 64 bytes) */
    if (!bsp_eeprom_read_blocking(EEPROM_HEADER_OFFSET, (uint8_t *)&hdr, sizeof(hdr)))
    {
        return 0u; /* I2C no respondió */
    }

    /* validar magic + version + count antes de leer más */
    if (hdr.magic != EEPROM_MAGIC)
        return 0u;
    if (hdr.version != EEPROM_VERSION)
        return 0u;
    if (hdr.count > NEIGHBOURHOOD_WHITELIST_MAX)
        return 0u;

    /* leer todos los entries a 400 kHz en Fast Mode */
    if (!bsp_eeprom_read_blocking(EEPROM_ENTRIES_OFFSET, entries_buf, sizeof(entries_buf)))
    {
        return 0u;
    }

    /* reconstruir el header útil (sin crc) y validar CRC sobre header + count×16 */
    hdr_for_crc[0] = (uint8_t)(EEPROM_MAGIC & 0xFFu);
    hdr_for_crc[1] = (uint8_t)((EEPROM_MAGIC >> 8) & 0xFFu);
    hdr_for_crc[2] = (uint8_t)((EEPROM_MAGIC >> 16) & 0xFFu);
    hdr_for_crc[3] = (uint8_t)((EEPROM_MAGIC >> 24) & 0xFFu);
    hdr_for_crc[4] = EEPROM_VERSION;
    hdr_for_crc[5] = hdr.count;
    hdr_for_crc[6] = 0u;
    hdr_for_crc[7] = 0u;

    calc_crc = crc16_ccitt_continue(0xFFFFu, hdr_for_crc, 8u);
    calc_crc = crc16_ccitt_continue(calc_crc, entries_buf, (uint16_t)(hdr.count * EEPROM_ENTRY_SIZE));
    if (calc_crc != hdr.crc)
        return 0u; /* corrupción detectada */

    /* bloque válido, sobrescribir la whitelist en RAM */
    neighbourhood_whitelist_clear();
    for (i = 0u; i < hdr.count; i++)
    {
        memcpy(numero, &entries_buf[i * EEPROM_ENTRY_SIZE], NEIGHBOURHOOD_PHONE_LEN_MAX);
        numero[NEIGHBOURHOOD_PHONE_LEN_MAX] = '\0';
        neighbourhood_whitelist_append_unchecked(numero);
    }
    return 1u;
}

/* -------------------------------------------------------------------------------------*/
// APIs públicas:

void eeprom_init(void)
{
    fsm_eeprom = ST_EEPROM_IDLE;
    tick_eeprom = 0;
    current_page = 0u;
    computed_crc = 0u;

    crc16_table_init(); /* arma la tabla CRC en RAM (~30 µs bloqueante) */
    bsp_eeprom_init();
    init_queue_event_task_eeprom();

    if (!op_load_from_eeprom())
    {
        /* si el chip está vacío o corrupto, entonces formato con la seed de RAM (cargada por
         * neighbourhood_init() antes de este init). */
        put_event_task_eeprom(EV_EEPROM_PERSIST_WHITELIST);
    }
}

/* -------------------------------------------------------------------------------------*/

void eeprom_update(void) // FSM
{
    switch (fsm_eeprom)
    {

    /* ─────────────────────────────────────────────────────────── */
    case ST_EEPROM_IDLE:
        if (any_event_task_eeprom())
        {
            (void)get_event_task_eeprom(); /* sólo hay un tipo de evento */
            current_page = 0u;
            fsm_eeprom = ST_EEPROM_PREPARE_CRC;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_EEPROM_PREPARE_CRC:
        computed_crc = op_compute_total_crc();
        fsm_eeprom = ST_EEPROM_BUILD_PAGE;
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_EEPROM_BUILD_PAGE:
        op_build_current_page();
        fsm_eeprom = ST_EEPROM_KICKOFF_PAGE;
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_EEPROM_KICKOFF_PAGE:
        op_kickoff_current_page();
        fsm_eeprom = ST_EEPROM_WRITING;
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_EEPROM_WRITING:
        if (bsp_eeprom_tx_done())
        {
            tick_eeprom = EEPROM_TWR_MS; /* tx terminado */
            fsm_eeprom = ST_EEPROM_WAIT_TWR;
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    case ST_EEPROM_WAIT_TWR:
        if (tick_eeprom > 0)
        {
            tick_eeprom--; /* contando... */
        }
        else
        {
            current_page++;
            if (current_page < EEPROM_TOTAL_PAGES)
            {
                fsm_eeprom = ST_EEPROM_BUILD_PAGE; /* siguiente página */
            }
            else
            {
                fsm_eeprom = ST_EEPROM_IDLE; /* flujo completo   */
            }
        }
        break;

    /* ─────────────────────────────────────────────────────────── */
    default:
        /* Acá no se llama a eeprom_init() (a diferencia de lo que se hace en las fsm anteriores
         * de por ejemplo sensores/actuadores). eeprom_init() es bloqueante y recalcula la tabla CRC. Eso
         * rompe el tick de 1 ms. Si fsm_eeprom se corrompe, cae a IDLE y el
         * próximo caller_add/remove encola un PERSIST nuevo. */
        fsm_eeprom = ST_EEPROM_IDLE;
        break;
    }
}
