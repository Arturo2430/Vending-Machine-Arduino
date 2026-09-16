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
#include "vm_motor_controller.h"

VmUartLink uartLink;
VmRecordStore recordStore;
VmMegaController controller;
VmMotorController motorController;

void motorStop();
bool motorIsBusy();
bool motorStart(uint8_t channel);
void motorPoll();
int motorConsumeResult();

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("--- MEGA (UART v2 / motores DC / PCA9685) ---");

    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);
    pinMode(VM_PIN_DOOR, INPUT_PULLUP);

    motorController.begin();

    VmMegaController::DispenserHooks hooks = {
        motorStop,
        motorIsBusy,
        motorStart,
        motorPoll,
        motorConsumeResult
    };

    Serial1.begin(VM_UART_BAUDRATE);
    uartLink.begin(Serial1);

    controller.setDispenser(hooks);
    controller.begin(uartLink, recordStore);
    controller.sendHelloHandshake();

    Serial.print("[MEGA] Modo inicial: ");
    Serial.println(
        controller.getMode() == VM_MODE_VENTA
            ? "VENTA"
            : "MANTENIMIENTO"
    );
}

void loop() {
    // Consume tramas UART y avanza el ciclo no bloqueante.
    controller.poll();
}
void motorStop() {
    motorController.stop();
}
bool motorIsBusy() {
    return motorController.isBusy();
}

bool motorStart(uint8_t channel) {
    return motorController.start(channel);
}

void motorPoll() {
    motorController.poll();
}

int motorConsumeResult() {
    return motorController.consumeResult();
}