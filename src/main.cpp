/**
 * @file main.cpp
 * @brief Firmware de la máquina expendedora (Arduino Mega 2560).
 *
 * Arquitectura: super-loop no bloqueante.
 *   - setup(): inicializa periféricos e inyecta dependencias en la FSM.
 *   - loop(): sondea el teclado y llama a fsm.update() en cada ciclo.
 *
 * Periféricos:
 *   Teclado 4x4  → D22-D29
 *   LCD 20x4 I2C → SDA/SCL (I2C)
 *   Motores DC   → PCA9685 (I2C)
 *   RFID MFRC522 → D53/D8 (SPI)
 *   Puerta       → D3  (INPUT_PULLUP)
 *   Barrera óptica → D2 (INPUT_PULLUP)
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
// Instancias globales de los subsistemas
// ---------------------------------------------------------------------------
static VmDisplay        display;
static VmEepromData     storage;
static VmMotorController motor;
static VmRfid           rfid;

// Prototipo de la función de display que se inyecta en la FSM
static void onDisplay(const char* l1, const char* l2,
                      const char* l3, const char* l4);

static VmFsm fsm(storage, motor, rfid, onDisplay);

static void onDisplay(const char* l1, const char* l2,
                      const char* l3, const char* l4) {
    display.show(l1, l2, l3, l4);
}

// ---------------------------------------------------------------------------
// Teclado matricial 4x4
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
// setup() y loop()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    Wire.begin(); // I2C debe iniciarse antes del LCD y PCA9685

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