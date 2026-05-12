/*
 * Módulo BLE (HM-10 / BT05 en USART1).
 * FSM de 6 estados: setup, espera de conexión, login doble factor,
 * sesión con comandos ADD/DEL/LIST/OUT, cierre forzado por alarma.
 *
 * Manda al sistema: EV_SYS_BLE_USER_CONNECTED, EV_SYS_BLE_USER_AUTHED,
 *                   EV_SYS_BLE_USER_DISCONNECTED  (ver task_system_interface.h).
 * Consume del sistema: EV_BLE_FORCE_CLOSE  (ver ble_interface.h).
 */

#ifndef BLE_H
#define BLE_H

void ble_init  (void);   /* se llama una vez en app_init()     */
void ble_update(void);   /* se llama cada tick en app_update() */

#endif
