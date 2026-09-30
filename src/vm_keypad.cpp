/**
 * @file vm_keypad.cpp
 * @brief Interpretación de teclas físicas a acciones semánticas.
 */

#include "vm_keypad.h"
#include <stdint.h>

// Monedas mexicanas en centavos, indexadas por SELECT_1..SELECT_9.
static const uint32_t COIN_VALUES[9] = {
    100u,   // '1' = $1.00
    200u,   // '2' = $2.00
    500u,   // '3' = $5.00
    1000u,  // '4' = $10.00
    2000u,  // '5' = $20.00
    5000u,  // '6' = $50.00
    10000u, // '7' = $100.00
    20000u, // '8' = $200.00
    50000u  // '9' = $500.00
};

VmKeypad::VmKeypad() {
}

KeyAction VmKeypad::interpret(char key, KeyMode mode) {

    // --- Pantalla de reposo: solo selección de canal 1-4 ---
    if (mode == KeyMode::REPOSO) {
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        return KeyAction::IGNORAR;
    }

    // --- Selección de método de pago: A=efectivo, B=RFID, *=cancelar ---
    if (mode == KeyMode::PAGO) {
        if (key == 'A') return KeyAction::CHOOSE_CASH;
        if (key == 'B') return KeyAction::CHOOSE_RFID;
        if (key == '*') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORAR;
    }

    // --- Insertando monedas: teclas 1-9 = denominaciones, B = cancelar ---
    if (mode == KeyMode::EFECTIVO) {
        if (key == 'B') return KeyAction::CANCEL_ABORT;
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        if (key == '5') return KeyAction::SELECT_5;
        if (key == '6') return KeyAction::SELECT_6;
        if (key == '7') return KeyAction::SELECT_7;
        if (key == '8') return KeyAction::SELECT_8;
        if (key == '9') return KeyAction::SELECT_9;
        return KeyAction::IGNORAR;
    }

    return KeyAction::IGNORAR;
}

uint32_t VmKeypad::coinActionToCentavos(KeyAction action) const {
    uint8_t idx = (uint8_t)action;
    uint8_t base = (uint8_t)KeyAction::SELECT_1;
    uint8_t top  = (uint8_t)KeyAction::SELECT_9;
    if (idx < base || idx > top) {
        return 0u;
    }
    return COIN_VALUES[idx - base];
}