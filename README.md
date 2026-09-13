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

Este repositorio contiene el código fuente para el microcontrolador periférico (Arduino Mega) de la máquina expendedora SAID.
Su responsabilidad principal es el control de hardware de bajo nivel (motores, teclado, LCD y sensores), actuando como esclavo bajo las órdenes del ESP32 mediante el protocolo UART v2.0.

### Funciones principales del firmware
- **Control de Actuadores:** Giro, calibración y parada no bloqueante de servos (canales 1 a 4).
- **Interfaz Física:** Captura estable de teclado matricial y refresco de pantalla LCD (16x2).
- **Sensores y Seguridad:** Lectura de puerta, barrera óptica y almacenamiento seguro en EEPROM para evitar cortes de energía.
- **Comunicación UART v2.0:** Recepción de comandos (`VEND`, `SET_MODE`, `DISPLAY`) y reporte de eventos físicos (`KEY`, `RESULT`, `STATUS`).

---

## Estructura del Proyecto

Para mantener el código organizado y modular, el repositorio sigue la estructura estándar de **PlatformIO / C++**:

| Directorio | Propósito |
| :--- | :--- |
| 📁 **`src/`** | Código fuente principal (`.cpp`, `.ino`). Punto de entrada `setup()` y `loop()`. |
| 📚 **`lib/`** | Librerías locales específicas del proyecto (ej. `ControladorServos`, `TecladoLCD`, `UartEsclavo`). |
| ⚙️ **`include/`** | Archivos de cabecera (`.h`) para configuraciones globales, pines (ej. `pines_mega.h`) y mapa de hardware. |

*(Nota: A diferencia del ESP32, el Arduino Mega no utiliza carpeta `data/` porque no maneja un sistema de archivos para web).*

---

## Subgrupo Arduino

El desarrollo de este firmware está a cargo de:

* **Diego Ramírez Ibarra:** Servos, estado neutral, calibración y parada no bloqueante.
* **Hugo Guillermo de Leon Ruiz:** Teclado y LCD para compra, PIN oculto y campos de reposición.
* **Eder Omar Zúñiga Zavala:** Implementación de UART v2 (esclavo), puerta, barrera, registro en EEPROM y `SET_MODE`.

---

<div align="center">

**Universidad Politécnica de Victoria · ITIID 7-1 · Septiembre - Diciembre 2026**

</div>