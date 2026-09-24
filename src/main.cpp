/**
 * @file main.cpp
 * @brief Firmware Arduino Mega 2.1.0 monoprocesador (sin ESP32).
 *
 * Wiring: teclado físico (Keypad lib) -> FSM; FSM -> motor PCA9685,
 * LCD 20x4 I2C y EEPROM. Toda la máquina de estados corre en este MCU.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include "vm_board_config.h"
#include "vm_display.h"
#include "vm_keypad.h"
#include "vm_eeprom_data.h"
#include "vm_motor_controller.h"
#include "vm_rfid.h"
#include "vm_fsm.h"

// ---------------------------------------------------------------------------
// Instancias globales
// ---------------------------------------------------------------------------
static VmDisplay display;
static VmEepromData storage;
static VmMotorController motor;
static VmRfid rfid;

static void displayEvent(const char* l1, const char* l2,
                         const char* l3, const char* l4);

static VmFsm fsm(storage, motor, rfid, displayEvent);

static void displayEvent(const char* l1, const char* l2,
                         const char* l3, const char* l4) {
    display.show(l1, l2, l3, l4);
}

// ---------------------------------------------------------------------------
// Teclado 4x4 (filas D22-D25, columnas D26-D29)
// ---------------------------------------------------------------------------
static char keypadMap[VM_KEYPAD_ROWS][VM_KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

static byte keypadRowPins[VM_KEYPAD_ROWS] = {
    VM_KEYPAD_ROW_0, VM_KEYPAD_ROW_1, VM_KEYPAD_ROW_2, VM_KEYPAD_ROW_3
};

static byte keypadColPins[VM_KEYPAD_COLS] = {
    VM_KEYPAD_COL_0, VM_KEYPAD_COL_1, VM_KEYPAD_COL_2, VM_KEYPAD_COL_3
};

static Keypad keypad(makeKeymap(keypadMap),
                     keypadRowPins,
                     keypadColPins,
                     VM_KEYPAD_ROWS,
                     VM_KEYPAD_COLS);

// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    Wire.begin(); // <-- INICIAR I2C ANTES DEL LCD

    pinMode(VM_PIN_DOOR, INPUT_PULLUP);
    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);

    display.begin();
    keypad.setDebounceTime(VM_KEYPAD_DEBOUNCE_MS);

    motor.begin();
    rfid.begin();
    fsm.begin();
}

void loop() {
    char key = keypad.getKey();
    if (key != NO_KEY) {
        fsm.handleKey(key);
    }

    fsm.update();
}