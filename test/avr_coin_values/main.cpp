#include <avr/pgmspace.h>
#include "vm_keypad.h"

// Reproduce la tabla identica que usa la semilla de EEPROM.
static const uint32_t seedValues[4] PROGMEM = {100u, 200u, 500u, 1000u};

volatile uint32_t coinValues[4];
volatile uint32_t referenceValues[4];
volatile uint32_t invalidValue;

// Punto de parada para inspeccionar resultados en el simulador AVR.
__attribute__((noinline, used)) void testComplete() {
    asm volatile("nop");
}

int main() {
    VmKeypad keypad;
    for (uint8_t i = 0; i < 4u; i++) {
        KeyAction action = keypad.interpret('1' + i, KeyMode::CASH);
        coinValues[i] = keypad.coinActionToCentavos(action);
        referenceValues[i] = pgm_read_dword(&seedValues[i]);
    }
    invalidValue = keypad.coinActionToCentavos(KeyAction::IGNORE_KEY);
    testComplete();
    for (;;) {}
}
