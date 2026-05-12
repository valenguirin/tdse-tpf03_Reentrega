/*
 * bsp_gpio.c Es el  Board Support Package: entradas y salidas digitales (HAL STM32F103)
 *
 * Es de los ÚNICOS(los bsp_*.c) archivo que incluye main.h y llama funciones HAL para GPIO.
 * Para portar a otra plataforma: reemplazar solo este archivo.
 */

/*
 * Flags de interrupción EXTI — escritos por HAL_GPIO_EXTI_Callback (ISR),
 * leídos y borrados por las funciones bsp_*_isr_triggered() (main loop).
 */

#include "main.h"
#include "bsp_gpio.h"

static volatile uint8_t panic_btn_isr_flag = 0;
static volatile uint8_t ring_isr_flag = 0;

// El loop principal lee y borra los flags con bsp_*_isr_triggered().

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == SENS_PANIC_BUTTON_Pin)
        panic_btn_isr_flag = 1u;
    else if (GPIO_Pin == RING_GSM_Pin)
        ring_isr_flag = 1u;
}

/* ────────────────────────────────────────────────────────────────────────── */
// Tiempo
uint32_t bsp_gpio_millis(void)
{
    return HAL_GetTick(); /* ms desde el arranque */
}

/* ────────────────────────────────────────────────────────────────────────── */
// Entradas digitales
/*
 * Botón de pánico (PB2), se activa con tensión 0V, pull-up externo
 *   Sin presionar : PB2 = HIGH. retorna 0
 *   Presionado    : PB2 = LOW . retorna 1
 */
uint8_t bsp_gpio_panic_btn_pressed(void)
{
    if (HAL_GPIO_ReadPin(SENS_PANIC_BUTTON_GPIO_Port, SENS_PANIC_BUTTON_Pin) == GPIO_PIN_RESET)
        return 1;
    return 0;
}

/*
 * LDR día/noche (PA1) , divisor resistivo
 *   Noche (oscuro) : LDR alta resistencia. PA1 = HIGH , retorna 1
 *   Día   (claro)  : LDR baja resistencia . PA1 = LOW  , retorna 0
 */
uint8_t bsp_gpio_ldr_is_night(void)
{
    if (HAL_GPIO_ReadPin(SENS_LDR_GPIO_Port, SENS_LDR_Pin) == GPIO_PIN_SET)
        return 1;
    return 0;
}

/* ────────────────────────────────────────────────────────────────────────── */
// Salidas digitales

/* LED azul (ACT_SIREN). PB0  activo en alto */
void bsp_gpio_led_blue(uint8_t on)
{
    if (on)
        HAL_GPIO_WritePin(ACT_SIREN_GPIO_Port, ACT_SIREN_Pin, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(ACT_SIREN_GPIO_Port, ACT_SIREN_Pin, GPIO_PIN_RESET);
}

/* LED blanco (ACT_STROBE). PB1  activo en alto */
void bsp_gpio_led_white(uint8_t on)
{
    if (on)
        HAL_GPIO_WritePin(ACT_STROBE_GPIO_Port, ACT_STROBE_Pin, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(ACT_STROBE_GPIO_Port, ACT_STROBE_Pin, GPIO_PIN_RESET);
}

/* LED amarillo (LED_BLE_STATUS). PA5  activo alto (indicador de red GSM) */
void bsp_gpio_led_alarm(uint8_t on)
{
    if (on)
        HAL_GPIO_WritePin(LED_BLE_STATUS_GPIO_Port, LED_BLE_STATUS_Pin, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(LED_BLE_STATUS_GPIO_Port, LED_BLE_STATUS_Pin, GPIO_PIN_RESET);
}

/* LED rojo (LED_ALARM_STATUS) . PA4  activo en alto */
void bsp_gpio_led_network(uint8_t on)
{
    if (on)
        HAL_GPIO_WritePin(LED_ALARM_STATUS_GPIO_Port, LED_ALARM_STATUS_Pin, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(LED_ALARM_STATUS_GPIO_Port, LED_ALARM_STATUS_Pin, GPIO_PIN_RESET);
}

/* ────────────────────────────────────────────────────────────────────────── */
// Getters de flags ISR
/*
 * bsp_gpio_btn_panic_isr_triggered ¿se disparó EXTI2 desde la última consulta?
 * Lee el flag, lo borra y retorna 1 si hubo flanco.
 * task_sensor_panic_btn.c(en su FSM)  llama esto cada tick .
 */
uint8_t bsp_gpio_panic_btn_isr_triggered(void)
{
    if (panic_btn_isr_flag)
    {
        panic_btn_isr_flag = 0u;
        return 1u;
    }
    return 0u;
}

/*
 * bsp_gpio_ring_isr_triggered ¿se disparó EXTI9_5 en PB5 (RING GSM)?
 * Lee el flag, lo borra y retorna 1 si hubo flanco descendente.
 * gsm.c (en su FSM) llama esto cada tick para detectar llamada entrante.
 */
uint8_t bsp_gpio_ring_isr_triggered(void)
{
    if (ring_isr_flag)
    {
        ring_isr_flag = 0u;
        return 1u;
    }
    return 0u;
}
