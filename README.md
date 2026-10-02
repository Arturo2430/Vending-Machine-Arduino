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
máquina de estados corre en un único **Arduino Mega 2560**, sin ESP32, sin
comunicación UART y sin web/WiFi.

El Mega implementa el negocio completo: selección de producto, pago en
**efectivo** (monedas simuladas con el teclado) o con **tarjeta RFID**,
despacho por motores DC, cálculo de cambio, reembolso en caso de falla y
carrusel de promociones en LCD 20x4. El inventario y la caja se guardan en la
EEPROM interna.

> El menú de administración (PIN) ya no forma parte de este firmware.

## Módulos

| Módulo | Responsabilidad |
| :--- | :--- |
| `src/main.cpp` | Wiring: teclado físico (`Keypad`), LCD I2C, motor, RFID, EEPROM y FSM. |
| `vm_fsm.*` | Máquina de estados de compra y reembolso (S0-S16). |
| `vm_eeprom_data.*` | Persistencia en EEPROM: 4 slots (producto, precio, stock) y caja de monedas. Valida los datos al arrancar y reinicializa con los datos semilla si no son válidos. |
| `vm_motor_config.*` / `vm_motor_controller.*` | Motores DC vía **PCA9685**: `start/poll/stop`, barrera como detección de caída (SUCCESS) y timeout hacia `UNCERTAIN`. |
| `vm_keypad.*` | Interpretación de teclas por modo (reposo, pago, efectivo, mensaje). |
| `vm_display.*` | LCD **20x4 I2C** (0x27) vía `marcoschwartz/LiquidCrystal_I2C`. |
| `vm_carousel.*` | Carrusel de subpantallas (2 s por pantalla). |
| `vm_change_calculator.*` | Cambio y reembolso con las monedas de la caja ($10/$5/$2/$1). |
| `vm_rfid.*` | Lector **MFRC522**: lectura, cobro y devolución de saldo (MIFARE Classic, bloque 4). |

## Diagrama de flujo (FSM)

```
S0 Arranque -> S2 Reposo            (EEPROM falla -> S1 Error interno)
S2 Reposo (1-4 elige canal) -> S3 Selección de canal
S3 (A confirma / * cancela) -> S4 Selección de pago
S4 (A=Efectivo, B=RFID, * cancela) -> S5 Efectivo | S6 RFID
S5 Monedas (1=$1 2=$2 3=$5 4=$10, * cancela); monto >= precio -> S7
S6 Tarjeta; saldo suficiente y cobro OK -> S7
S7 Reserva stock + arranca motor -> S8 (si falla -> S10)
S8 Despachando (poll motor) -> S9 Confirmada | S10 Falla
S9 -> S11 Cambio (efectivo) | S12 Pantalla fin (RFID)
S11 -> S12 Pantalla fin -> S2       (sin monedas para cambio -> S13 + adeudo)
S10 Falla -> S14 Reembolso RFID | S15 Reembolso en monedas
S14 -> S16 (si falla la escritura -> S15)
S15 -> S16 Pantalla fin de reembolso -> S2   (sin monedas -> S13 + adeudo)
S13 Mensaje (A continúa) -> destino del mensaje (S2 o S4)
```

Cancelar en S5 con monedas insertadas (`*` o inactividad) también pasa por S15.

## Hardware (configurable en `vm_board_config.h`)

- **Barrera óptica:** D2 (INPUT_PULLUP, LOW = producto detectado) — el motor la usa para detectar la caída (entregado) e impedir un nuevo giro si está ocupada.
- **Teclado 4x4:** filas D22-D25, columnas D30, D31, D33 y D29 (`Keypad` lib).
- **LCD:** I2C 20x4, dirección `0x27`.
- **Motores DC:** PCA9685 (I2C, 0x40), canales PWM en `vm_motor_config.h`.
- **RFID MFRC522:** SPI (SS=D53, RST=D8, SCK=D52, MISO=D50, MOSI=D51).

## Datos semilla (primer arranque de la EEPROM)

| Slot | Producto | Precio | Stock | Capacidad |
| :--- | :--- | :--- | :--- | :--- |
| 1 | Coca-Cola | $18.00 | 8 | 10 |
| 2 | Galletas Marias | $15.00 | 6 | 10 |
| 3 | Agua 600ml | $12.00 | 9 | 10 |
| 4 | Jugo Naranja | $14.00 | 5 | 10 |

- Caja inicial: 10 piezas de $10, $5, $2 y $1 (máximo 250 por denominación).
- Las monedas insertadas se suman a la caja al momento de insertarlas; el
  cambio y los reembolsos se descuentan de la misma caja.
- La EEPROM se reinicializa con estos datos cuando cambia `VERSION` en
  `vm_eeprom_data.cpp` o cuando los datos guardados no son válidos.
- Si no hay monedas para devolver, el monto queda como **adeudo** y se muestra
  en el reposo. El adeudo vive en RAM: se pierde al reiniciar.

## Estructura del Proyecto

| Directorio | Propósito |
| :--- | :--- |
| 📁 **`src/`** | Implementaciones: `main.cpp`, `vm_fsm.cpp`, `vm_eeprom_data.cpp`, `vm_motor_controller.cpp`, `vm_keypad.cpp`, `vm_display.cpp`, `vm_carousel.cpp`, `vm_change_calculator.cpp`, `vm_rfid.cpp`. |
| ⚙️ **`include/`** | Cabeceras y parámetros de placa (`vm_board_config.h`). |
| 📄 **`docs/`** | Manual de usuario y reporte técnico de la FSM. |

Dependencias externas (`lib_deps`): `Adafruit PWM Servo Driver Library`,
`Keypad`, `LiquidCrystal_I2C` y `MFRC522`.

## Compilar y cargar

```bash
pio run                 # compilar
pio run -t upload       # compilar y cargar por USB
```

---

<div align="center">

**Universidad Politécnica de Victoria · ITIID 7-1 · Septiembre - Diciembre 2026**

</div>
