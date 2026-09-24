/**
 * @file vm_change_calculator.h
 * @brief Cálculo de cambio usando monedas de la caja (portado del ESP32).
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
     * Calcula el cambio para `paid - price` usando las denominaciones de la
     * caja. Devuelve true si se formó el cambio y descuenta las monedas.
     */
    bool calculate(uint32_t paidCentavos, uint32_t priceCentavos,
                   VmEepromData& cashBox, ChangeResult& result);
};

#endif // VM_CHANGE_CALCULATOR_H