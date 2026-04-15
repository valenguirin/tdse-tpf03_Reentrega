<p align="center">
  <img src="./img/logo-fiuba.png" alt="image1">
</p>

<p align="center">
  <strong>UNIVERSIDAD DE BUENOS AIRES</strong>
</p>

<p align="center">
  <strong>Facultad de Ingeniería</strong>
</p>

<p align="center">
  <strong>86.65/TA134 Taller de Sistemas Embebidos</strong>
</p>

# Statecharts y diagramas de secuencia: Alarma Vecinal 

<table align="center">
  <tr>
    <th>Autor</th>
    <th>Padrón</th>
  </tr>
  <tr>
    <td>Valentín Alexis Guirin</td>
    <td>107416</td>
  </tr>
  <tr>
    <td>Yerson Monzón Alayo</td>
    <td>104262</td>
  </tr>
  <tr>
    <td>Carolina Gonzales Peralta</td>
    <td>110804</td>
  </tr>
</table>

<p align="center">
  <em>Este trabajo fue realizado en la Ciudad Autónoma de Buenos Aires, entre diciembre de 2025 y marzo de 2026.</em>
</p>

---

##  Índice General
- [Resumen](#resumen)
- [1 Lógica de Control (FSM)](#1-lógica-de-control-fsm)
   - [1.1 FSM del sistema (Sistema Global)](#11-fsm-del-sistema-global-sistema-global)
   - [1.2 FSM del botón de pánico](#12-fsm-del-botón-de-pánico)
   - [1.3 FSM del sensor LDR](#13-fsm-del-sensor-ldr)
   - [1.4 FSM del dispositivo GSM (sim800l)](#14-fsm-del-dispositivo-gsm)
   - [1.5 FSM del dispositivo BLE](#15-fsm-del-dispositivo-ble)
   - [1.6 FSM del LED azul](#16-fsm-del-led-azul)
   - [1.7 FSM del LED blanco](#17-fsm-del-led-blanco)
   - [1.8 FSM del LED rojo](#18-fsm-del-led-rojo)
   - [1.9 FSM del LED amarillo](#19-fsm-del-led-amarillo)
- [2 Diseño del Firmware](#2-diseño-del-firmware)
   - [2.1 Diagramas de Secuencia](#21-diagramas-de-secuencia)


---

##  Resumen
En el presente trabajo se diseñó e implementó una alarma vecinal para solucionar la problemática de inseguridad en barrios de la Ciudad de Buenos Aires. Mediante una llamada desde su teléfono móvil, teléfono fijo, así como presionando un botón de pánico, los usuarios pueden activar alertas sonoras y lumínicas. El sistema permite el mantenimiento vía BLE con doble factor de autenticación y gestión de whitelist; los cambios realizados se guardan en una memoria EEPROM externa.

## 1 Lógica de Control (FSM)
Aquí se detallan las máquinas de estados finitos que gobiernan el comportamiento del sistema.

### 1.1 FSM del sistema (Sistema Global)
Gestiona el paso entre alarma activa, mandar alertas y administración de la whitelist.

<p align="center">
  <img src="./img/system.png" alt="FSM del sistema">
</p>


### 1.2 FSM del botón de pánico
Lógica de antirrebotes físicos del botón de activación de la alarma.

<p align="center">
  <img src="./img/sensor_panic_button.png" alt="FSM PB">
</p>




### 1.3 FSM del sensor LDR
Lógica de validación de la luz ambiental para determinar si es necesario activar la luz estroboscópica.

<p align="center">
  <img src="./img/sensor_ldr.png" alt="FSM LDR">
</p>


### 1.4 FSM del dispositivo GSM (sim800l)
Controla la inicialización del módulo, el registro en la red y la gestión de llamadas entrantes.

<p align="center">
  <img src="./img/gsm.png" alt="FSM GSM">
</p>

### 1.5 FSM del dispositivo BLE
Gestiona los usuarios que pueden editar la whitelist.

<p align="center">
  <img src="./img/ble.png" alt="FSM BLE">
</p>


### 1.6 FSM del LED azul
Caracteriza una sirena sonora, se activa ya sea de día o de noche

<p align="center">
  <img src="./img/actuator_led_blue.png" alt="FSM BLED">
</p>

### 1.7 FSM del LED blanco
Caracteriza una luz estroboscópica, se activa solo de noche.
<p align="center">
  <img src="./img/actuator_led_white.png" alt="FSM WLED">
</p>



### 1.8 FSM del LED rojo
Es un LED que da información de que la alarma está activada y puede ser accionada en cualquier momento.

<p align="center">
  <img src="./img/actuator_led_red.png" alt="FSM RLED">
</p>



### 1.9 FSM del LED amarillo
Es un led que se queda prendido siempre que el gsm esté conectado a la red y entonces puede recibir llamadas y mandar las alertas por mensaje de texto a la policía.

<p align="center">
  <img src="./img/actuator_led_red.png" alt="FSM YLED">
</p>

##  2 Diseño del Firmware
### 2.1 Diagramas de Secuencia
Los diagramas de secuencia ilustran la interacción temporal entre los módulos:

* **Interacción entre los sensores y el sistema:** 
<p align="center">
  <img src="./img/sensor_to_system.png" alt="SENSTOSYS">
</p>

* **Interacción desde el módulo GSM hacia el sistema:** 
<p align="center">
  <img src="./img/gsm_to_system.png" alt="GSMTOSYS">
</p>

* **Interacción desde el sistema hacia el módulo GSM:** <p align="center">
  <img src="./img/system_to_gsm.png" alt="SYSTOGSM">
</p>

* **Interacción desde el módulo BLE hacia el sistema:** 
<p align="center">
  <img src="./img/BLE_to_SYS.png" alt="BLETOSYS">
</p>

* **Interacción desde el sistema hacia el módulo BLE:** <p align="center">
  <img src="./img/SYS_to_BLE.png" alt="SYSTOBLE">
</p>
