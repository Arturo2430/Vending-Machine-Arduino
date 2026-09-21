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
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
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
LiquidCrystal_I2C lcd(0x27, VM_DISPLAY_LINE_LEN, VM_DISPLAY_LINE_COUNT);

char keypadMap[VM_KEYPAD_ROWS][VM_KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

byte keypadRows[VM_KEYPAD_ROWS] = {
    VM_KEYPAD_ROW_0, VM_KEYPAD_ROW_1, VM_KEYPAD_ROW_2, VM_KEYPAD_ROW_3
};

byte keypadCols[VM_KEYPAD_COLS] = {
    VM_KEYPAD_COL_0, VM_KEYPAD_COL_1, VM_KEYPAD_COL_2, VM_KEYPAD_COL_3
};

Keypad keypad = Keypad(makeKeymap(keypadMap), keypadRows, keypadCols,
                       VM_KEYPAD_ROWS, VM_KEYPAD_COLS);

void displaySink(const char* line1, const char* line2,
                 const char* line3, const char* line4) {
    const char* lines[VM_DISPLAY_LINE_COUNT] = { line1, line2, line3, line4 };

    lcd.clear();
    for (uint8_t row = 0; row < VM_DISPLAY_LINE_COUNT; row++) {
        lcd.setCursor(0, row);
        for (uint8_t column = 0; column < VM_DISPLAY_LINE_LEN; column++) {
            lcd.write((uint8_t)lines[row][column]);
        }
    }
}

void motorStop();
bool motorIsBusy();
bool motorStart(uint8_t channel);
void motorPoll();
int motorConsumeResult();

void setup() {
    Serial.begin(115200);

    Serial.println("--- MEGA (UART v2 / motores DC / PCA9685) ---");

    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);
    pinMode(VM_PIN_DOOR, INPUT_PULLUP);

    Wire.begin();
    lcd.init();
    lcd.backlight();

    motorController.begin();
    controller.beginRfid();
    keypad.setDebounceTime(VM_KEYPAD_DEBOUNCE_MS);

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
    controller.setDisplaySink(displaySink);
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

    char key = keypad.getKey();
    if (key != NO_KEY) {
        controller.sendKeyEvent(key);
    }
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