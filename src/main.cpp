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

static VmDisplay         display;
static VmEepromData      storage;
static VmMotorController motor;
static VmRfid            rfid;

// Puente de impresion hacia la pantalla
static void onDisplay(const char* l1, const char* l2, const char* l3, const char* l4) {
    display.show(l1, l2, l3, l4);
}

static VmFsm fsm(storage, motor, rfid, onDisplay);

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

// Instancia global del teclado matricial
static Keypad keypad(makeKeymap(keypadMap), keypadRowPins, keypadColPins, VM_KEYPAD_ROWS, VM_KEYPAD_COLS);

void setup() {
    Serial.begin(115200);

    // Iniciar I2C antes que sus modulos esclavos
    Wire.begin();

    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);

    display.begin();
    keypad.setDebounceTime(VM_KEYPAD_DEBOUNCE_MS);

    motor.begin();
    rfid.begin();
    fsm.begin();
}

void loop() {
    if (keypad.getKeys()) {
        for (uint8_t i = 0; i < LIST_MAX; i++) {
            // Procesar unicamente transiciones a pulsado
            if (keypad.key[i].stateChanged && keypad.key[i].kstate == PRESSED) {
                fsm.handleKey(keypad.key[i].kchar);
            }
        }
    }

    fsm.update();
}
