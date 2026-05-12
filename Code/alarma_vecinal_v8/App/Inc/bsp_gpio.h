#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

/*
 * bsp_gpio.h Es el Board Support Package de entradas y salidas digitales
 *
 * Único archivo que conoce el hardware GPIO.
 * Sirve para portar a otra plataforma: y solo tener que reemplazar bsp_gpio.c.
 *
 * Convención de retorno para entradas digitales:
 *   1 = activo (presionado, detectado)
 *   0 = inactivo
 *
 * Convención para salidas:
 *   on = 1 -> encender / activar
 *   on = 0 ->apagar  / desactivar
 */

/* ── Tiempo ──────────────────────────────────────────────────────────────── */
uint32_t bsp_gpio_millis(void); /* ms desde el arranque              */

/* ── Entradas ────────────────────────────────────────────────────────────── */
uint8_t bsp_gpio_panic_btn_pressed(void); /* botón de pánico — PB2 (polling)   */
uint8_t bsp_gpio_ldr_is_night(void);      /* LDR oscuridad   — PA1 (1=noche)   */

/* ── Entradas por interrupción (EXTI) ────────────────────────────────────── */
uint8_t bsp_gpio_panic_btn_isr_triggered(void); /* EXTI2   — PB2, flanco bajada      */
uint8_t bsp_gpio_ring_isr_triggered(void);      /* EXTI9_5 — PB5, RING GSM, activo bajo */

/* ── Salidas ─────────────────────────────────────────────────────────────── */
void bsp_gpio_led_blue(uint8_t on);    /* LED azul    (sirena)   — PB0      */
void bsp_gpio_led_white(uint8_t on);   /* LED blanco  (estrobo)  — PB1      */
void bsp_gpio_led_alarm(uint8_t on);   /* LED rojo    (alarma)   — PA4      */
void bsp_gpio_led_network(uint8_t on); /* LED amarillo (red GSM) — PA5      */

#endif /* BSP_GPIO_H */
