#ifndef VM_KEYPAD_H
#define VM_KEYPAD_H

#include <stdint.h>

enum class KeyMode : uint8_t {
    REPOSO   = 0,
    PAGO     = 1,
    EFECTIVO = 2
};

enum class KeyAction : uint8_t {
    IGNORAR      = 0,
    CANCEL_ABORT = 1,
    SELECT_1     = 4,
    SELECT_2     = 5,
    SELECT_3     = 6,
    SELECT_4     = 7,
    CHOOSE_CASH  = 17,
    CHOOSE_RFID  = 18
};

class VmKeypad {
public:
    VmKeypad();
    KeyAction interpret(char key, KeyMode mode);
    uint32_t coinActionToCentavos(KeyAction action) const;
};

#endif // VM_KEYPAD_H