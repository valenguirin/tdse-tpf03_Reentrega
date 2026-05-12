/*
 * wcet.c: medición de WCET usando el DWT Cycle Counter (ARM Cortex-M)

 * DWT->CYCCNT: contador de ciclos de CPU, 32 bits, incrementa cada ciclo.
 * No usa HAL ni timers por lo que su impacto es mínimo en el sistema.


 */

#include "wcet.h"
#include "stm32f1xx.h" // DWT

/* ── Frecuencia del micro ────────────────────────────────────────────────── */

#define CPU_FREQ_MHZ 64u /* STM32F103RBT6 a 64 MHz  */

/* ── Variables públicas ─────────────────────────────────────────────────── */

uint32_t wcet_max[WCET_COUNT] = {0};

/* ── Variables privadas ─────────────────────────────────────────────────── */

static uint32_t t_start = 0;

/* ── API ─────────────────────────────────────────────────────────────────── */

void wcet_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* habilitar tracing  */
    DWT->CYCCNT = 0;                                /* resetear contador  */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            /* iniciar conteo     */
}

void wcet_start(void)
{
    __disable_irq(); /* ninguna IRQ contamina la medición */
    t_start = DWT->CYCCNT;
}

void wcet_stop(Wcet_Id id)
{
    uint32_t ciclos = DWT->CYCCNT - t_start;
    uint32_t us = ciclos / CPU_FREQ_MHZ;
    __enable_irq(); /* IRQs pendientes disparan acá, entre tasks */

    if (us > wcet_max[id])
        wcet_max[id] = us;
}
