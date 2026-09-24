/**
 * @file vm_change_calculator.cpp
 * @brief Implementación del calculador de cambio (greedy, portado del ESP32).
 */

#include <Arduino.h>
#include "vm_change_calculator.h"

VmChangeCalculator::VmChangeCalculator() {
}

bool VmChangeCalculator::calculate(uint32_t paidCentavos,
                                   uint32_t priceCentavos,
                                   VmEepromData& cashBox,
                                   ChangeResult& result) {
    result.coin1000 = 0;
    result.coin500 = 0;
    result.coin200 = 0;
    result.coin100 = 0;

    if (priceCentavos == 0u || paidCentavos < priceCentavos) {
        return false;
    }

    uint32_t total = paidCentavos - priceCentavos;
    if (total == 0u) {
        return true;
    }

    const uint32_t denominations[4] = { 1000u, 500u, 200u, 100u };
    uint32_t* counters[4] = { &result.coin1000, &result.coin500,
                              &result.coin200, &result.coin100 };

    for (uint8_t i = 0; i < 4 && total > 0u; i++) {
        uint32_t available = 0;
        if (!cashBox.getCoinStock(denominations[i], available)) {
            available = 0;
        }

        uint32_t use = 0;
        while (use < available && total >= denominations[i]) {
            total -= denominations[i];
            use++;
        }

        if (use > 0u) {
            if (!cashBox.deductCoins(denominations[i], use)) {
                Serial.println(F("CHANGE_ERROR_CANT_DEDUCT"));
                return false;
            }
            *counters[i] = use;
        }
    }

    if (total > 0u) {
        Serial.println(F("CHANGE_ERROR_CALC_NOT_POSSIBLE"));
        return false;
    }

    return true;
}