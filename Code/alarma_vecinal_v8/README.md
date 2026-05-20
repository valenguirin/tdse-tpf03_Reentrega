# Alarma Vecinal — STM32F103RB

Nodo de alarma comunitaria sobre NUCLEO-F103RB: botón de pánico con antirebote, sensor LDR día/noche, comunicación celular (SIM800L: llamadas y SMS), comunicación BLE (HM-10), persistencia en EEPROM I²C (24LC256) y cuatro indicadores LED. Arquitectura cooperativa de un único tick de **1 ms**, sin RTOS, organizada en capas: **App / Tasks / Interfaces / BSP**.

---

## 1. Requisitos

|                   |                                                                    |
| ----------------- | ------------------------------------------------------------------ |
| IDE               | **STM32CubeIDE 1.17.0** (versión con la que se desarrolló y probó) |
| Placa             | NUCLEO-F103RB (STM32F103RBT6, 128 KB Flash / 20 KB RAM)            |
| Reloj del sistema | **64 MHz**                                                         |

## 2. Guía de lectura para corregir

**Convención:** para cada módulo, leer primero el `.h` y después el `.c` (implementación). Los archivos están ordenados por funcionalidad: si se sigue este orden, cada archivo se apoya en lo anterior.

| #   | Archivo principal                                                                                                                                                                                                                                                                                                                    | Por qué leerlo aquí                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| --- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | [Core/Src/main.c](Core/Src/main.c)                                                                                                                                                                                                                                                                                                   | **Punto de entrada.** Lo único que hace el `while(1)` es llamar a `app_update()`. Todo lo demás es inicialización del HAL generada por CubeMX.                                                                                                                                                                                                                                                                                                                                  |
| 2   | [App/Inc/app.h](App/Inc/app.h) + [App/Src/app.c](App/Src/app.c)                                                                                                                                                                                                                                                                      | **Coordinador.** Define el tick de 1 ms y el orden fijo de las tres fases: **SCRUTINIZE** (sensores leen) → **PROCESS** (sistema decide) → **ACT** (actuadores ejecutan). Se ve la lista completa de tareas.                                                                                                                                                                                                                                                                    |
| 3   | [App/Inc/task_sensor_panic_btn.h](App/Inc/task_sensor_panic_btn.h) + [App/Src/task_sensor_panic_btn.c](App/Src/task_sensor_panic_btn.c)                                                                                                                                                                                              | **Primer sensor — antirebote.** La FSM más simple del proyecto: BUTTON_UP → BUTTON_FALLING → BUTTON_DOWN → BUTTON_RAISING. Buen primer contacto con el patrón "FSM por archivo + estado privado". Lee el hardware vía [bsp_gpio.h](App/Inc/bsp_gpio.h) (`bsp_gpio_panic_btn_pressed()`).                                                                                                                                                                                        |
| 4   | [App/Inc/task_sensor_ldr.h](App/Inc/task_sensor_ldr.h) + [App/Src/task_sensor_ldr.c](App/Src/task_sensor_ldr.c)                                                                                                                                                                                                                      | **Segundo sensor — histéresis.** Mismo patrón que el botón pero con histéresis para no oscilar en el umbral día/noche. También va contra `bsp_gpio`.                                                                                                                                                                                                                                                                                                                            |
| 5   | [App/Inc/task_system.h](App/Inc/task_system.h) + [App/Src/task_system.c](App/Src/task_system.c) + [App/Inc/task_system_interface.h](App/Inc/task_system_interface.h) + [App/Src/task_system_interface.c](App/Src/task_system_interface.c)                                                                                            | **El cerebro.** `task_system_interface` es la **cola de eventos** que los sensores y los módulos de comunicación encolan; `task_system` es la FSM principal que la consume y decide qué actuadores activar, a quién llamar por GSM o qué responder por BLE.                                                                                                                                                                                                                     |
| 6   | LEDs: [task_act_led_blue](App/Inc/task_act_led_blue.h) · [task_act_led_white](App/Inc/task_act_led_white.h) · [task_act_led_alarm](App/Inc/task_act_led_alarm.h) · [task_act_led_network](App/Inc/task_act_led_network.h) + [task_actuator_interface.h](App/Inc/task_actuator_interface.h) / [.c](App/Src/task_actuator_interface.c) | **Actuadores.** Mismo molde que las tasks de sensor pero al revés: reciben comandos vía `task_actuator_interface` y manejan el patrón de parpadeo. Con leer **una** se entienden las cuatro.                                                                                                                                                                                                                                                                                    |
| 7   | GSM: [gsm.h](App/Inc/gsm.h) + [gsm.c](App/Src/gsm.c) + [gsm_interface.h](App/Inc/gsm_interface.h) / [.c](App/Src/gsm_interface.c)                                                                                                                                                                                                    | **Comunicación celular (SIM800L).** FSM grande con sub-estados: arranque del módem, registro a la red, detección de llamada entrante (RING), parseo de CLIP (número llamante) contra la whitelist, envío de SMS. Apoyarse en [bsp_uart_gsm.h](App/Inc/bsp_uart_gsm.h) (DMA TX + RX por IDLE IRQ — todo no bloqueante) y [bsp_utilities.h](App/Inc/bsp_utilities.h) (normaliza el prefijo +54 contra la whitelist). Config centralizada en [gsm_config.h](App/Inc/gsm_config.h). |
| 8   | BLE: [ble.h](App/Inc/ble.h) + [ble.c](App/Src/ble.c) + [ble_interface.h](App/Inc/ble_interface.h) / [.c](App/Src/ble_interface.c)                                                                                                                                                                                                    | **Comunicación BLE (HM-10).** FSM de comandos por línea: la app móvil manda comandos AT-like, el HM-10 los pasa por UART, y `ble.c` los autentica (ver [auth_utils.h](App/Inc/auth_utils.h)) y ejecuta. Apoyarse en [bsp_uart_ble.h](App/Inc/bsp_uart_ble.h) (RX DMA circular con parser por línea). Config en [ble_config.h](App/Inc/ble_config.h).                                                                                                                            |
| 9   | EEPROM: [eeprom.h](App/Inc/eeprom.h) + [eeprom.c](App/Src/eeprom.c) + [eeprom_interface.h](App/Inc/eeprom_interface.h) / [.c](App/Src/eeprom_interface.c)                                                                                                                                                                            | **Persistencia I²C (24LC256).** Lee la whitelist de números al arrancar y la reescribe cuando BLE la modifica. Usa write por DMA con espera de tWR. Apoyarse en [bsp_eeprom.h](App/Inc/bsp_eeprom.h) y en la whitelist en RAM definida por [config_neighbourhood.h](App/Inc/config_neighbourhood.h).                                                                                                                                                                            |

---

## 4. La capa BSP : qué hace y por qué existe

Los archivos `bsp_*` son el **Board Support Package**: la **única capa que toca hardware directamente** (HAL de STM32, registros, pines, DMAs, IRQs). El resto del código habla con los periféricos a través de ellos. Si mañana se migrara este firmware a otra placa o a otro micro, en teoría sólo habría que reescribir los BSPs y todo lo de arriba quedaría igual.

| BSP                                                | Qué hace                                                                                                                                                        |
| -------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [bsp_gpio](App/Inc/bsp_gpio.h)                     | Pines digitales (botón pánico, LDR, los 4 LEDs, sirena, estrobo) + `bsp_gpio_millis()` que es la base de tiempo para todo el sistema.                           |
| [bsp_uart_gsm](App/Inc/bsp_uart_gsm.h)             | USART3 al SIM800L. TX por DMA1 Canal 2. RX por DMA1 Canal 3 + UART IDLE IRQ. **Cero llamadas bloqueantes** — un `HAL_UART_Transmit` clásico congelaría el tick. |
| [bsp_uart_ble](App/Inc/bsp_uart_ble.h)             | USART1 al HM-10. TX DMA + RX en DMA circular con parser por línea (el HM-10 manda comandos terminados en `\r\n`).                                               |
| [bsp_uart_callbacks](App/Src/bsp_uart_callbacks.c) | Punto único donde el HAL entrega las interrupciones de las dos UARTs y las despacha al BSP que corresponda. Evita ensuciar `stm32f1xx_it.c`.                    |
| [bsp_eeprom](App/Inc/bsp_eeprom.h)                 | I²C1 al 24LC256. Read bloqueante sólo en init (antes del scheduler), write por DMA1 Canal 6 con espera de tWR.                                                  |
| [bsp_utilities](App/Inc/bsp_utilities.h)           | Utilidades específicas del despliegue regional (Argentina): normalización de números (`+549XXX` ↔ `XXX`) para comparar contra la whitelist.                     |

---

## 5. Mediciones WCET : 20% de CPU

Las mediciones se hicieron en las situaciones de estrés del sistema: activación por botón de pánico y por llamada, SMS a todos los destinatarios, llamada de un número no registrado, alta a un número nuevo y activación por este nuevo número, activación de sirena y luz estroboscópica, ejecutadas sobre hardware con SIM y antena GSM operativas en una sesión previa.

**La unidad de los valores en la captura:** todos los números del array "wcet_max[]" están expresados en **microsegundos (µs)**. La conversión de ciclos DWT a µs la hace "wcet_stop()" dividiendo por la frecuencia del core (64 MHz), ver [App/Src/wcet.c](App/Src/wcet.c).

### Captura wcet_max[] leído desde Live Expressions

![WCET por tarea — Live Expressions del array wcet_max[] definido en App/Inc/wcet.h](docs/img/wcet_live_expressions.png)

### Cálculo del factor de utilización U

Con período de tick: $T$ $= 1 ms = 1000 µs$ , el factor de utilización es:

$$U=\sum_{i=0}^{N-1}\frac{\text{wcet\_max}[i]}{T}$$

Mapeando los índices de la captura contra el `enum Wcet_Id` de [App/Inc/wcet.h](App/Inc/wcet.h):

| Índice | Tarea (`Wcet_Id`)                | `wcet_max[i]` (µs) | Contribución a U (`/1000`) |
| :----: | -------------------------------- | :----------------: | :------------------------: |
|   0    | `WCET_SENSOR_BTN_PANIC`          |         4          |           0.004            |
|   1    | `WCET_SENSOR_LDR`                |         4          |           0.004            |
|   2    | `WCET_GSM`                       |         72         |           0.072            |
|   3    | `WCET_BLE`                       |         51         |           0.051            |
|   4    | `WCET_EEPROM`                    |         51         |           0.051            |
|   5    | `WCET_SYSTEM`                    |         9          |           0.009            |
|   6    | `WCET_ACT_LED_BLUE`              |         3          |           0.003            |
|   7    | `WCET_ACT_LED_WHITE`             |         2          |           0.002            |
|   8    | `WCET_ACT_LED_ALARM`             |         2          |           0.002            |
|   9    | `WCET_ACT_LED_NETWORK`           |         2          |           0.002            |
|        | **Σ tiempo de cómputo por tick** |     **200 µs**     |         **0.200**          |

$$U = \frac{4 + 4 + 72 + 51 + 51 + 9 + 3 + 2 + 2 + 2}{1000} = \frac{200}{1000} = 0{,}20 = \mathbf{20\,\%}$$

El sistema consume **20 % de la CPU** en peor caso. El **80 % restante del tick (800 µs cada milisegundo) queda libre** para nuevas tareas o margen ante eventos no medidos, sin riesgo de perder el tick de 1 ms.

### Escenario sin SIM ni antena GSM

Como el equipo no dispone físicamente de la SIM ni de la antena GSM en esta semana, se agregan dos escenarios reproducibles hoy mismo para que la metodología sea verificable. La tabla de estrés del 20 % corresponde a la sesión previa con el chip operativo y se incorporará también a la memoria del TPF.

#### Escenario A - Sistema en reposo

GSM apagado, BLE sin app central conectada. Todas las FSMs descansan en su estado de espera.

![wcet_max[] en reposo total](docs/img/wcet_init_0.png)

| Índice | Tarea (`Wcet_Id`)                | `wcet_max[i]` (µs) | Contribución a U (`/1000`) |
| :----: | -------------------------------- | :----------------: | :------------------------: |
|   0    | `WCET_SENSOR_BTN_PANIC`          |         3          |           0.003            |
|   1    | `WCET_SENSOR_LDR`                |         4          |           0.004            |
|   2    | `WCET_GSM`                       |         11         |           0.011            |
|   3    | `WCET_BLE`                       |         6          |           0.006            |
|   4    | `WCET_EEPROM`                    |         1          |           0.001            |
|   5    | `WCET_SYSTEM`                    |         4          |           0.004            |
|   6    | `WCET_ACT_LED_BLUE`              |         1          |           0.001            |
|   7    | `WCET_ACT_LED_WHITE`             |         1          |           0.001            |
|   8    | `WCET_ACT_LED_ALARM`             |         2          |           0.002            |
|   9    | `WCET_ACT_LED_NETWORK`           |         1          |           0.001            |
|        | **Σ tiempo de cómputo por tick** |     **34 µs**      |         **0.034**          |

$$U_A = \frac{3 + 4 + 11 + 6 + 1 + 4 + 1 + 1 + 2 + 1}{1000} = \frac{34}{1000} = 0{,}034 = \mathbf{3{,}4\,\%}$$

#### Escenario B - GSM encendido pero sin SIM ni antena

SIM800L alimentado pero sin SIM ni antena. La FSM del GSM ejecuta `AT+CMGF`, `AT+CLIP` y `AT+CREG` en loop sin completar el registro a red.

![wcet_max[] inicial sin SIM ni antena GSM](docs/img/wcet_init.png)

| Índice | Tarea (`Wcet_Id`)                | `wcet_max[i]` (µs) | Contribución a U (`/1000`) |
| :----: | -------------------------------- | :----------------: | :------------------------: |
|   0    | `WCET_SENSOR_BTN_PANIC`          |         3          |           0.003            |
|   1    | `WCET_SENSOR_LDR`                |         4          |           0.004            |
|   2    | `WCET_GSM`                       |         29         |           0.029            |
|   3    | `WCET_BLE`                       |         21         |           0.021            |
|   4    | `WCET_EEPROM`                    |         2          |           0.002            |
|   5    | `WCET_SYSTEM`                    |         4          |           0.004            |
|   6    | `WCET_ACT_LED_BLUE`              |         1          |           0.001            |
|   7    | `WCET_ACT_LED_WHITE`             |         1          |           0.001            |
|   8    | `WCET_ACT_LED_ALARM`             |         2          |           0.002            |
|   9    | `WCET_ACT_LED_NETWORK`           |         1          |           0.001            |
|        | **Σ tiempo de cómputo por tick** |     **68 µs**      |         **0.068**          |

$$U_B = \frac{3 + 4 + 29 + 21 + 2 + 4 + 1 + 1 + 2 + 1}{1000} = \frac{68}{1000} = 0{,}068 = \mathbf{6{,}8\,\%}$$

---

## 6. Pinout resumido

| Pin         | Función                                                    | Periférico          |
| ----------- | ---------------------------------------------------------- | ------------------- |
| PB2         | Botón de pánico (pulsador externo, polling con antirebote) | GPIO                |
| PA1         | Sensor LDR (entrada digital + histéresis)                  | GPIO                |
| PB10 / PB11 | TX / RX hacia SIM800L                                      | USART3 + DMA1 ch2/3 |
| PA9 / PA10  | TX / RX hacia HM-10                                        | USART1 + DMA1 ch4/5 |
| PB5         | Línea RING del SIM800L (indica llamada entrante)           | GPIO IRQ            |
| PB4         | BLE_STATE del HM-10 (1 = conectado)                        | GPIO                |
| PB6 / PB7   | SCL / SDA hacia EEPROM 24LC256                             | I²C1 + DMA1 ch6     |
| PA4         | LED estado alarma                                          | GPIO out            |
| PA5         | LED estado BLE                                             | GPIO out            |
| PB0 / PB1   | Sirena / Estrobo (relés de potencia)                       | GPIO out            |
| PC13        | B1 (botón usuario de la NUCLEO, opcional)                  | GPIO                |
| PA2 / PA3   | USART2 — VCP de ST-Link (debug por consola)                | USART2              |
