// Calculo de cambio usando denominaciones de la caja

#ifndef VM_CHANGE_CALCULATOR_H
#define VM_CHANGE_CALCULATOR_H

#include <stdint.h>
#include "vm_eeprom_data.h"

// Distribucion de monedas para dar cambio
typedef struct {
    uint32_t coin1000;
    uint32_t coin500;
    uint32_t coin200;
    uint32_t coin100;
} ChangeResult;

// Calculadora de cambio y reembolsos
class VmChangeCalculator {
public:
    VmChangeCalculator();

    // Devuelve true si hay monedas suficientes para el cambio (paid - price)
    // Si price = 0 calcula el reembolso completo
    bool calculate(uint32_t paidCentavos, uint32_t priceCentavos,
                   VmEepromData& cashBox, ChangeResult& result);
};

#endif // VM_CHANGE_CALCULATOR_H