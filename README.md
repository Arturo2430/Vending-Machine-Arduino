<div align="center">

# Firmware Arduino Mega - Máquina Expendedora

[![Arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white)](#)
[![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](#)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-F56600?style=for-the-badge&logo=platformio&logoColor=white)](#)

**ITIID 7-1 · Septiembre - Diciembre 2026**

*Docente: Dr. Said Polanco Martagón*

</div>

---

## Resumen

Este repositorio contiene la parte del firmware para el microcontrolador periférico (Arduino Mega) de la máquina expendedora SAID correspondiente a **Eder Omar Zúñiga Zavala**.

Su responsabilidad (PDF 1, Programación Arduino):
- **UART v2 (extremo Mega):** implementa el contrato `SAID-ARCH-UART` v2.0.0 FINAL como nodo esclavo: HELLO, ACK, STATUS, SET_MODE, HEARTBEAT, DISPLAY, VEND y RESULT, con parser incremental no bloqueante.
- **Idempotencia y rechazo de duplicados:** un mismo `transaction_id` nunca vuelve a girar un motor; duplicado con canal distinto se rechaza con `DUPLICATE_CONFLICT`.
- **Puerta (D3) y barrera óptica (D2):** lectura con debounce y reporte en STATUS; la barrera obstruida detiene el inicio del giro (`REJECTED_BEFORE_MOTION`).
- **Registro EEPROM:** conserva el último resultado físico para recuperación y reconciliación ante cortes de energía (Q20).
- **SET_MODE:** transición VENTA/MANTENIMIENTO con protección `BUSY` y arranque obligatorio en MODE_MANTENIMIENTO (UART-REQ-003).

El Mega **no** implementa negocio: no consulta SQLite, no autoriza pagos ni interpreta PIN (eso pertenece al ESP32).

### Integración con los otros subgrupos del Mega

- **Servos (Diego Ramírez):** se conectan vía `VmMegaController::setDispenser()`. Sin actuador registrado, una orden VEND se confirma y se cierra como `REJECTED_BEFORE_MOTION`.
- **Teclado y LCD (Hugo de León):** se conectan vía `VmMegaController::setDisplaySink()` (texto DISPLAY) y `VmMegaController::sendKeyEvent()` (mensaje KEY).

---

## Estructura del Proyecto

La estructura sigue la convención de **PlatformIO / C++**:

| Directorio | Propósito |
| :--- | :--- |
| 📁 **`src/`** | Código fuente del proyecto: `main.cpp`, `vm_mega_controller.cpp` (rol esclavo, puerta, barrera y SET_MODE) y `vm_record_store.cpp` (registro EEPROM). |
| 📚 **`lib/`** | `VmUartLink`: capa de enlace y protocolo UART compartidos (idéntica al repo ESP32). |
| ⚙️ **`include/`** | Cabeceras del proyecto (`vm_board_config.h`, `vm_mega_controller.h`, `vm_record_store.h`). |

> `lib/VmUartLink/vm_uart_protocol.h` debe mantenerse **idéntico** al del repositorio `Vending-Machine-ESP32` (UART-REQ-001).

---

## Subgrupo Arduino (estado actual del repo)

* **Eder Omar Zúñiga Zavala:** UART v2, puerta, barrera, registro EEPROM y `SET_MODE`. *(implementado aquí)*
* **Diego Ramírez Ibarra:** Servos, neutral, calibración y parada no bloqueante. *(pendiente, se integra por hooks)*
* **Hugo Guillermo de Leon Ruiz:** Teclado y LCD para compra, PIN oculto y campos de reposición. *(pendiente, se integra por hooks)*

---

<div align="center">

**Universidad Politécnica de Victoria · ITIID 7-1 · Septiembre - Diciembre 2026**

</div>