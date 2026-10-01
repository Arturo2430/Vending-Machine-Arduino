#include "vm_keypad.h"

// Centavos por denominacion, mapeados a SELECT_1..SELECT_4
static const uint32_t COIN_VALUES[4] = {
    100u, 200u, 500u, 1000u
};

VmKeypad::VmKeypad() {
}

KeyAction VmKeypad::interpret(char key, KeyMode mode) {
    if (mode == KeyMode::STANDBY) {
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        return KeyAction::IGNORE_KEY;
    }

    if (mode == KeyMode::PAYMENT) {
        if (key == 'A') return KeyAction::CHOOSE_CASH;
        if (key == 'B') return KeyAction::CHOOSE_RFID;
        if (key == '*') return KeyAction::CANCEL_ABORT;
        return KeyAction::IGNORE_KEY;
    }

    if (mode == KeyMode::CASH) {
        if (key == '*') return KeyAction::CANCEL_ABORT;
        if (key == '1') return KeyAction::SELECT_1;
        if (key == '2') return KeyAction::SELECT_2;
        if (key == '3') return KeyAction::SELECT_3;
        if (key == '4') return KeyAction::SELECT_4;
        return KeyAction::IGNORE_KEY;
    }

    if (mode == KeyMode::PROMPT) {
        if (key == 'A') return KeyAction::CONTINUE;
        return KeyAction::IGNORE_KEY;
    }

    return KeyAction::IGNORE_KEY;
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