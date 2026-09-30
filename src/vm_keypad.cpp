/**
 * @file vm_keypad.cpp
 * @brief Interpretación de teclas físicas a acciones semánticas.
 */

#include "vm_keypad.h"
#include <stdint.h>

// Monedas en centavos (máximo $10.00), indexadas por SELECT_1..SELECT_4.
static const uint32_t COIN_VALUES[4] = {
    100u,  // '1' = $1.00
    200u,  // '2' = $2.00
    500u,  // '3' = $5.00
    1000u  // '4' = $10.00
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

    // --- Insertando monedas: teclas 1-4 ($1/$2/$5/$10), B = cancelar ---
    if (mode == KeyMode::EFECTIVO) {
        if (key == 'B') return KeyAction::CANCEL_ABORT;
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        return KeyAction::IGNORAR;
    }

    return KeyAction::IGNORAR;
}

uint32_t VmKeypad::coinActionToCentavos(KeyAction action) const {
    uint8_t idx  = (uint8_t)action;
    uint8_t base = (uint8_t)KeyAction::SELECT_1;
    uint8_t top  = (uint8_t)KeyAction::SELECT_4;
    if (idx < base || idx > top) {
        return 0u;
    }
    return COIN_VALUES[idx - base];
}