/**
 * @file vm_keypad.h
 * @brief Interpretación semántica del teclado 4x4.
 *
 * La exploración física del teclado la realiza la librería `Keypad` en
 * main.cpp. Este módulo solo traduce el carácter físico a una acción
 * con significado según el modo actual de la FSM.
 */

#ifndef VM_KEYPAD_H
#define VM_KEYPAD_H

#include <stdint.h>

/* Contexto de la pantalla actual (cambia qué teclas son válidas). */
enum class KeyMode : uint8_t {
    REPOSO   = 0,  // Pantalla de inicio: seleccionar canal 1-4
    PAGO     = 1,  // Seleccionar forma de pago (efectivo o RFID)
    EFECTIVO = 2   // Insertando monedas: teclas 1-9 = denominaciones
};

/* Acción semántica resultante de presionar una tecla en un modo dado. */
enum class KeyAction : uint8_t {
    IGNORAR      = 0,
    CANCEL_ABORT = 1,  // 'B' en compra / '*' en selección de pago
    SELECT_1     = 4,
    SELECT_2     = 5,
    SELECT_3     = 6,
    SELECT_4     = 7,
    SELECT_5     = 8,
    SELECT_6     = 9,
    SELECT_7     = 10,
    SELECT_8     = 11,
    SELECT_9     = 12,
    CHOOSE_CASH  = 17, // 'A' → pagar con efectivo
    CHOOSE_RFID  = 18  // 'B' → pagar con tarjeta RFID
};

class VmKeypad {
public:
    VmKeypad();

    /* Convierte un carácter físico al KeyAction correspondiente al modo dado. */
    KeyAction interpret(char key, KeyMode mode);

    /* Devuelve el valor en centavos de la moneda asociada a una acción SELECT_N. */
    uint32_t coinActionToCentavos(KeyAction action) const;

private:
    // (sin estado interno necesario)
};

#endif // VM_KEYPAD_H