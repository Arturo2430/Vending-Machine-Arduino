/**
 * @file main.cpp
 * @brief Punto de entrada del firmware Arduino Mega - parte de Eder.
 *
 * Alcance (PDF 1, Programación Arduino): UART v2 (extremo Mega), puerta,
 * barrera, registro EEPROM y SET_MODE. El controlador operativo se deja en
 * MODE_MANTENIMIENTO (UART-REQ-003) y espera que el ESP32 emita
 * SET_MODE(MODE_VENTA) para habilitar ventas.
 *
 * Los servos (Diego) y el teclado/LCD (Hugo) se conectan después mediante
 * VmMegaController::setDispenser() y setDisplaySink(), y el disparo de
 * VmMegaController::sendKeyEvent() desde el teclado.
 *
 * UART hacia el ESP32: Serial1 de hardware (TX1 D18, RX1 D19), niveles
 * adaptados por Electrónica. USB (D0/D1) queda para diagnóstico.
 */

#include <Arduino.h>
#include "vm_board_config.h"
#include "vm_uart_protocol.h"
#include "vm_uart_link.h"
#include "vm_record_store.h"
#include "vm_mega_controller.h"

VmUartLink uartLink;
VmRecordStore recordStore;
VmMegaController controller;

void setup() {
    Serial.begin(115200);  // USB de diagnóstico
    delay(1000);

    Serial.println("--- MEGA (Eder: UART v2 / puerta / barrera / EEPROM) ---");

    // Enlace UART con el ESP32.
    Serial1.begin(VM_UART_BAUDRATE);
    uartLink.begin(Serial1);

    // Controlador esclavo: recupera registro EEPROM, queda en
    // MODE_MANTENIMIENTO y concilia una orden "en progreso" si la hubo.
    controller.begin(uartLink, recordStore);

    // Handshake de arranque identificándose como ROLE_MEGA.
    controller.sendHelloHandshake();

    Serial.print("[MEGA] Modo inicial: ");
    Serial.println(controller.getMode() == VM_MODE_VENTA ? "VENTA" : "MANTENIMIENTO");
}

void loop() {
    // Consume tramas UART y avanza el ciclo no bloqueante.
    controller.poll();
}