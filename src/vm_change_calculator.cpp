/**
 * @file vm_change_calculator.cpp
 * @brief Calculo de cambio o reembolso con las monedas de la caja.
 */

#include <Arduino.h>
#include "vm_change_calculator.h"

static const uint8_t  DENOM_COUNT = 4;
static const uint32_t DENOMS[DENOM_COUNT] = { 1000u, 500u, 200u, 100u };

// Algoritmo de backtracking para dar cambio exacto con el inventario disponible
static bool findCoins(uint32_t total, uint8_t idx, const uint32_t avail[DENOM_COUNT], uint32_t use[DENOM_COUNT]) {
    uint32_t denom = DENOMS[idx];

    // Ultima denominacion: debe cubrir el resto exacto
    if (idx == DENOM_COUNT - 1u) {
        if ((total % denom) != 0u || (total / denom) > avail[idx]) {
            return false;
        }
        use[idx] = total / denom;
        return true;
    }

    uint32_t qty = total / denom;
    if (qty > avail[idx]) qty = avail[idx];

    for (;;) {
        use[idx] = qty;
        if (findCoins(total - qty * denom, idx + 1u, avail, use)) {
            return true;
        }
        if (qty == 0u) return false;
        qty--;
    }
}

VmChangeCalculator::VmChangeCalculator() {
}

// Con priceCentavos = 0 se calcula un reembolso del monto pagado
bool VmChangeCalculator::calculate(uint32_t paidCentavos, uint32_t priceCentavos, VmEepromData& cashBox, ChangeResult& result) {
    result.coin1000 = 0;
    result.coin500  = 0;
    result.coin200  = 0;
    result.coin100  = 0;

    if (paidCentavos < priceCentavos) {
        return false;
    }

    uint32_t total = paidCentavos - priceCentavos;
    if (total == 0u) {
        return true;
    }

    uint32_t avail[DENOM_COUNT];
    uint32_t use[DENOM_COUNT];
    for (uint8_t i = 0; i < DENOM_COUNT; i++) {
        if (!cashBox.getCoinStock(DENOMS[i], avail[i])) {
            avail[i] = 0u;
        }
        use[i] = 0u;
    }

    // Verifica disponibilidad antes de descontar para mantener consistencia
    if (!findCoins(total, 0u, avail, use)) {
        Serial.println(F("CHANGE_ERROR_CALC_NOT_POSSIBLE"));
        return false;
    }

    for (uint8_t i = 0; i < DENOM_COUNT; i++) {
        if (use[i] > 0u && !cashBox.deductCoins(DENOMS[i], use[i])) {
            Serial.println(F("CHANGE_ERROR_CANT_DEDUCT"));
            return false;
        }
    }

    result.coin1000 = use[0];
    result.coin500  = use[1];
    result.coin200  = use[2];
    result.coin100  = use[3];
    return true;
}
