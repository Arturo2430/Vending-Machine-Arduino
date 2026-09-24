/**
 * @file vm_keypad.cpp
 * @brief Interpretación de teclas (portado de vm_keypad.cpp del ESP32).
 */

#include "vm_keypad.h"
#include <stddef.h>
#include <stdint.h>

// Mapa de monedas: índice de tecla '1'..'9' -> centavos.
static const uint32_t COIN_VALUES[9] = {
    100u, 200u, 500u, 1000u, 2000u, 5000u, 10000u, 20000u, 50000u
};

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static uint8_t digitOf(char c) {
    return (uint8_t)(c - '0');
}

VmKeypad::VmKeypad() : _lastDigit(0) {
}

KeyAction VmKeypad::interpret(char key, KeyMode mode) {
    if (mode == KeyMode::REPOSO) {
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        if (key == 'A') return KeyAction::ENTER_ADMIN;
        return KeyAction::IGNORAR;
    }

    if (mode == KeyMode::PAGO) {
        if (key == 'A') return KeyAction::CHOOSE_CASH;
        if (key == 'B') return KeyAction::CHOOSE_RFID;
        if (key == '*') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORAR;
    }

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

    if (mode == KeyMode::ADMIN_PIN) {
        if (isDigit(key)) {
            _lastDigit = digitOf(key);
            return KeyAction::SELECT_NUM;
        }
        _lastDigit = 0;
        if (key == 'C') return KeyAction::CLEAR_BACKSPACE;
        if (key == 'A') return KeyAction::CONFIRM;
        if (key == 'B') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORAR;
    }

    if (mode == KeyMode::ADMIN_MENU) {
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        if (key == '5') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORAR;
    }

    if (mode == KeyMode::ADMIN_ACCION) {
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == 'B') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORAR;
    }

    if (mode == KeyMode::NUMERICO) {
        if (isDigit(key)) {
            _lastDigit = digitOf(key);
            return KeyAction::SELECT_NUM;
        }
        _lastDigit = 0;
        if (key == 'C') return KeyAction::CLEAR_BACKSPACE;
        if (key == 'A') return KeyAction::CONFIRM;
        if (key == 'B') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORAR;
    }

    return KeyAction::IGNORAR;
}

uint32_t VmKeypad::coinActionToCentavos(KeyAction action) const {
    uint8_t idx = (uint8_t)action;
    if (idx < (uint8_t)KeyAction::SELECT_1 || idx > (uint8_t)KeyAction::SELECT_9) {
        return 0u;
    }
    return COIN_VALUES[idx - (uint8_t)KeyAction::SELECT_1];
}