/*
 * Módulo GSM (SIM800L en USART3).
 * FSM de 14 estados: setup AT (CLIP, CMGF), polling de red, llamadas
 * entrantes con whitelist, envío iterativo de SMS.
 *
 * Manda al sistema: EV_SYS_GSM_ON_NETWORK, EV_SYS_GSM_OFF_NETWORK,
 *                   EV_SYS_CALL_AUTHORIZED  (ver task_system_interface.h).
 * Consume del sistema: EV_GSM_SEND_SMS  (ver gsm_interface.h).
 */

#ifndef GSM_H
#define GSM_H

void gsm_init(void);   /* se llama una vez en app_init()     */
void gsm_update(void); /* se llama cada tick en app_update() */

#endif
