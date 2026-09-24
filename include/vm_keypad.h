/**
 * @file vm_keypad.h
 * @brief Semántica del teclado 4x4 (Hugo de León), portada del ESP32.
 *
 * Solo interpreta teclas; la exploración física queda a cargo de la librería
 * `Keypad` (chris--a) que se usa en `main.cpp` con los pines de
 * `vm_board_config.h`.
 */

#ifndef VM_KEYPAD_H
#define VM_KEYPAD_H

#include <stdint.h>

enum class KeyMode : uint8_t {
    REPOSO        = 0,
    PAGO          = 1,   // Seleccionar forma de pago
    EFECTIVO      = 2,   // Insertando monedas
    ADMIN_PIN     = 3,
    ADMIN_MENU    = 4,
    ADMIN_ACCION  = 5,   // Elegir acción para un canal (precio/stock)
    NUMERICO      = 6    // Capturando valor numérico (precio o stock)
};

enum class KeyAction : uint8_t {
    IGNORAR       = 0,
    CANCEL_ABORT  = 1,   // B en compra; * en pago; B en admin = salir
    CLEAR_BACKSPACE = 2, // C en admin
    CONFIRM       = 3,   // A en admin
    SELECT_1      = 4,
    SELECT_2      = 5,
    SELECT_3      = 6,
    SELECT_4      = 7,
    SELECT_5      = 8,
    SELECT_6      = 9,
    SELECT_7      = 10,
    SELECT_8      = 11,
    SELECT_9      = 12,
    SELECT_0      = 13,
    SELECT_ASTERISK = 14,
    SELECT_NUM    = 15,
    ENTER_ADMIN   = 16,
    CHOOSE_CASH   = 17
};

class VmKeypad {
public:
    VmKeypad();

    /** Interpreta caractér de tecla físico y contexto; 2a es de 0 a 9. */
    KeyAction interpret(char key, KeyMode mode);

    /** Moneda de 0 a 9 centavos (moneda mexicana de 10 a 1000). */
    uint32_t coinActionToCentavos(KeyAction action) const;

    /** Dígito (0-9) de la última tecla numérica, si emitío SELECT_NUM. */
    uint8_t lastDigit() const { return _lastDigit; }

private:
    uint8_t _lastDigit;
};

#endif // VM_KEYPAD_H