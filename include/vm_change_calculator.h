/**
 * @file vm_change_calculator.h
 * @brief Cálculo de cambio con denominaciones de la caja de efectivo.
 */

#ifndef VM_CHANGE_CALCULATOR_H
#define VM_CHANGE_CALCULATOR_H

#include <stdint.h>
#include "vm_eeprom_data.h"

typedef struct {
    uint32_t coin1000;
    uint32_t coin500;
    uint32_t coin200;
    uint32_t coin100;
} ChangeResult;

class VmChangeCalculator {
public:
    VmChangeCalculator();

    /**
     * Calcula el cambio para `paid - price` usando las monedas de la caja.
     * Con price = 0 calcula el reembolso de `paid`.
     * Devuelve true y descuenta las monedas solo si el monto se puede formar;
     * si no, la caja queda sin cambios.
     */
    bool calculate(uint32_t paidCentavos, uint32_t priceCentavos,
                   VmEepromData& cashBox, ChangeResult& result);
};

#endif // VM_CHANGE_CALCULATOR_H