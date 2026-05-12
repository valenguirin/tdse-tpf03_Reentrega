/*
 * app.c

 * Único archivo que coordina todos los módulos.
 * main.c solo llama app_init() y app_update().

 * Orden fijo cada tick (1 ms):
    1. SCRUTINIZE: sensores leen hardware y encolan eventos en task_system_interface
    2. PROCESS : sistema consume la cola de eventos y decide
    3. ACT : actuadores ejecutan las decisiones
 */
#include "bsp_gpio.h" //no deberia tener acceso a esto, solo accede por el tiempo
#include "config_neighbourhood.h"

#include "gsm.h"

#include "ble.h"

#include "eeprom.h"

#include "app.h"

#include "task_sensor_panic_btn.h"
#include "task_sensor_ldr.h"

#include "task_system.h"

#include "task_act_led_blue.h"
#include "task_act_led_white.h"
#include "task_act_led_alarm.h"
#include "task_act_led_network.h"

#include "wcet.h" /* comentar para no medir WCET */

static uint32_t ultimo_tick = 0;

void app_init(void)
{
    /* config del sitio (whitelist editable en RAM) */
    neighbourhood_init();

    /* persistencia (sobreescribe la whitelist con la de EEPROM si es válida) */
    eeprom_init();

    /* sensores */
    sensor_panic_btn_init();
    sensor_ldr_init();

    /*Dispositivos de comunicación */
    gsm_init();
    ble_init();

    /* actuadores */
    act_led_blue_init();
    act_led_white_init();
    act_led_alarm_init();
    act_led_network_init();

    /* sistema */
    system_init();

    wcet_init();

    ultimo_tick = bsp_gpio_millis();
}

void app_update(void)
{
    if (bsp_gpio_millis() == ultimo_tick)
    {
        return;
    }
    ultimo_tick = bsp_gpio_millis();

    /* 1: SCRUTINIZE */
    wcet_start();
    sensor_panic_btn_update();
    wcet_stop(WCET_SENSOR_BTN_PANIC);

    wcet_start();
    sensor_ldr_update();
    wcet_stop(WCET_SENSOR_LDR);

    wcet_start();
    gsm_update();
    wcet_stop(WCET_GSM);

    wcet_start();
    ble_update();
    wcet_stop(WCET_BLE);

    wcet_start();
    eeprom_update();
    wcet_stop(WCET_EEPROM);

    /* 2: PROCESS */
    wcet_start();
    system_update();
    wcet_stop(WCET_SYSTEM);

    /* 3: ACT */
    wcet_start();
    act_led_blue_update();
    wcet_stop(WCET_ACT_LED_BLUE);

    wcet_start();
    act_led_white_update();
    wcet_stop(WCET_ACT_LED_WHITE);

    wcet_start();
    act_led_alarm_update();
    wcet_stop(WCET_ACT_LED_ALARM);

    wcet_start();
    act_led_network_update();
    wcet_stop(WCET_ACT_LED_NETWORK);
}
