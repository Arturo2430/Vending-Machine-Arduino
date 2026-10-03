#ifndef VM_KEYPAD_H
#define VM_KEYPAD_H

#include <stdint.h>

// Modos de operacion del teclado
enum class KeyMode : uint8_t {
    STANDBY = 0,
    PAYMENT = 1,
    CASH    = 2,
    PROMPT  = 3
};

// Acciones mapeadas desde las teclas fisicas
enum class KeyAction : uint8_t {
    IGNORE_KEY   = 0,
    CANCEL_ABORT = 1,
    SELECT_1     = 4,
    SELECT_2     = 5,
    SELECT_3     = 6,
    SELECT_4     = 7,
    CHOOSE_CASH  = 17,
    CHOOSE_RFID  = 18,
    CONTINUE     = 19
};

// Traductor de teclas fisicas a acciones logicas
class VmKeypad {
public:
    VmKeypad();
    KeyAction interpret(char key, KeyMode mode);
    uint32_t coinActionToCentavos(KeyAction action) const;
};

#endif // VM_KEYPAD_H