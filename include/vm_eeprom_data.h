/**
 * @file vm_eeprom_data.h
 * @brief Persistencia en EEPROM del estado de la máquina expendedora.
 *
 * Almacena:
 *   - 4 slots de producto (nombre, precio, stock, capacidad, habilitado).
 *   - Caja de efectivo: 4 denominaciones de moneda.
 *   - Registro de la última orden (recuperación ante cortes de energía).
 */

#ifndef VM_EEPROM_DATA_H
#define VM_EEPROM_DATA_H

#include <stdint.h>
#include "vm_types.h"

/* Información pública de un slot de producto. */
struct SlotInfo {
    char     productName[17]; // hasta 16 caracteres + terminador
    uint32_t priceCentavos;
    uint32_t stock;
    uint32_t capacity;
    bool     enabled;
};

class VmEepromData {
public:
    /* Registro de la última orden (para recuperación ante corte de energía). */
    struct Record {
        bool    inProgress;
        uint8_t channel;
        uint8_t result;      // vm_dispense_result_t
    };

    VmEepromData();

    /* Inicializa o carga el estado desde EEPROM. Devuelve false si hay falla. */
    bool begin();

    // ---- Slots de producto --------------------------------------------------
    bool getSlot(uint8_t slotId, SlotInfo& out) const;
    bool reserveStock(uint8_t slotId);   // stock > 0 ? stock-- : false
    void releaseStock(uint8_t slotId);   // devuelve la unidad al stock

    // ---- Caja de efectivo ---------------------------------------------------
    bool getCoinStock(uint32_t denomCentavos, uint32_t& outStock) const;
    bool addCoins(uint32_t denomCentavos, uint32_t count);
    bool deductCoins(uint32_t denomCentavos, uint32_t count);

    // ---- Registro transaccional ---------------------------------------------
    void loadRecord(Record& out) const;
    void beginOrder(uint8_t channel);
    void completeOrder(uint8_t channel, uint8_t result);

private:
    struct Slot {
        char     name[17];
        uint32_t price;
        uint8_t  stock;
        uint8_t  cap;
        uint8_t  enabled;
    };

    struct Cash {
        uint32_t denom;
        uint8_t  qty;
    };

    Slot   _slots[VM_CHANNEL_MAX];
    Cash   _cash[4];
    Record _record;

    void seedCache();
    void loadAll();
    void persistAll();
    bool hasMagic() const;
    bool verifyPersist() const;

    void persistSlot(uint8_t index);
    void persistCash(uint8_t index);
    void persistRecord();
};

#endif // VM_EEPROM_DATA_H