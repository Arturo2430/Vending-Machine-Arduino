<div align="center">

# Firmware Arduino Mega - Máquina Expendedora (2.1.0 monoprocesador)

[![Arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white)](#)
[![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](#)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-F56600?style=for-the-badge&logo=platformio&logoColor=white)](#)

**ITIID 7-1 · Septiembre - Diciembre 2026**

*Docente: Dr. Said Polanco Martagón*

</div>

---

## Resumen

Firmware **2.1.0 monoprocesador** para la máquina expendedora SAID: toda la
máquina de estados corre en un único **Arduino Mega**, sin ESP32, sin
comunicación UART, sin web/WiFi y sin RFID.

El Mega implementa el negocio completo portado de `Vending-Machine-ESP32`:
selección de producto, pago en efectivo, despacho por motores DC, cálculo de
cambio, carrousel de promociones en LCD 20x4 y menú de administración
(EEPROM local).

## Módulos

| Módulo | Responsabilidad |
| :--- | :--- |
| `src/main.cpp` | Wiring: teclado físico (`Keypad`), LCD I2C, motor, EEPROM y FSM. |
| `vm_fsm.*` | Máquina de estados de compra y administración (S0-S17, portada del ESP32). |
| `vm_eeprom_data.*` | Persistencia mínima en EEPROM: 4 slots, caja, PIN y última orden (Q20). |
| `vm_motor_config.*` / `vm_motor_controller.*` | Motores DC vía **PCA9685** (Diego): `start/poll/stop`, barrera como detección de caída (SUCCESS) y timeout hacia `UNCERTAIN`. |
| `vm_keypad.*` | Interpretación de teclas por modo (compra/efectivo/admin). |
| `vm_display.*` | LCD **20x4 I2C** (0x27) vía `marcoschwartz/LiquidCrystal_I2C`. |
| `vm_carousel.*` | Carrousel de subpantallas (2 s por pantalla). |
| `vm_change_calculator.*` | Cambio greedy con monedas de la caja (denominaciones $10/$5/$2/$1). |

## Diagrama de flujo (FSM)

```
S0 Arranque -> S2 Reposo
S2 Reposo (1-4 elige canal, A admin) -> S3 Sel canal
S3 (A confirma / * cancela) -> S4 Sel pago
S4 (A=Efectivo) -> S5 Efectivo
S5 Inserta monedas (1-9, B cancela); suficiente -> S7 Reservada
S7 Reserva stock + registra orden + valida puerta/barrera -> S8
S8 Despachando (poll motor) -> S9 Confirmada | S10 Falla
S9 -> S11 Cambio -> S12 Pantalla fin -> S2
S13 PIN admin -> S14 Canal -> S15 Accion -> S16 Precio / S17 Stock
```

## Hardware (configurable en `vm_board_config.h`)

- **Puerta:** D3 (INPUT_PULLUP, debounce 30 ms) — bloquea el despacho si está abierta.
- **Barrera óptica:** D2 — el motor la usa para detectar la caída (entregado) e impedir nuevo giro si está ocupada.
- **Teclado 4x4:** filas D22-D25, columnas D26-D29 (`Keypad` lib).
- **LCD:** I2C 20x4, dirección `0x27`.
- **Motores DC:** PCA9685 (I2C, 0x40), canales PWM en `vm_motor_config.h`.

## Datos semilla (primer arranque de la EEPROM)

| Slot | Producto | Precio | Stock | Capacidad |
| :--- | :--- | :--- | :--- | :--- |
| 1 | Coca-Cola 355 ml | $18.00 | 8 | 10 |
| 2 | Galletas Marías | $15.00 | 6 | 10 |
| 3 | Agua 600 ml | $12.00 | 9 | 10 |
| 4 | Jugo Naranja | $14.00 | 5 | 10 |

- Caja inicial: 10 piezas de $10, $5, $2 y $1.
- PIN de administrador: `1234` (3 fallos = bloqueo 30 s).

## Estructura del Proyecto

| Directorio | Propósito |
| :--- | :--- |
| 📁 **`src/`** | Implementaciones: `main.cpp`, `vm_fsm.cpp`, `vm_eeprom_data.cpp`, `vm_motor_controller.cpp`, `vm_keypad.cpp`, `vm_display.cpp`, `vm_carousel.cpp`, `vm_change_calculator.cpp`. |
| ⚙️ **`include/`** | Cabeceras y parámetros de placa (`vm_board_config.h`). |

Dependencias externas (`lib_deps`): `Adafruit PWM Servo Driver Library`,
`Keypad` y `LiquidCrystal_I2C`.

## Compilar y cargar

```bash
pio run                 # compilar
pio run -t upload       # compilar y cargar por USB
```

---

<div align="center">

**Universidad Politécnica de Victoria · ITIID 7-1 · Septiembre - Diciembre 2026**

</div>