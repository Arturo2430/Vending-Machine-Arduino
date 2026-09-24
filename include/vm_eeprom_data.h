/**
 * @file vm_eeprom_data.h
 * @brief Persistencia en EEPROM del estado mínimo de la máquina.
 *
 * Reemplaza la lógica SQLite del ESP32 y el registro EEPROM del contrato
 * UART en el esquema monoprocesador:
 *   - 4 slots (producto, precio, stock, capacidad, habilitado).
 *   - Caja de efectivo (4 denominaciones de moneda).
 *   - PIN de administrador.
 *   - Registro de la última orden física (recuperación ante corte, Q20).
 */

#ifndef VM_EEPROM_DATA_H
#define VM_EEPROM_DATA_H

#include <stdint.h>
#include "vm_types.h"

struct SlotInfo {
    char     productName[17];  // 16 caracteres + terminador
    uint32_t priceCentavos;
    uint32_t stock;
    uint32_t capacity;
    bool     enabled;
};

class VmEepromData {
public:
    struct Record {
        bool     inProgress;   // quedó una orden a media al cortar la luz
        uint8_t  channel;
        uint8_t  result;       // vm_dispense_result_t
    };

    VmEepromData();

    /**
     * Carga el estado persistido, o lo inicializa con datos semilla si la
     * EEPROM está vacía/corrupta. true si quedó operativo.
     */
    bool begin();

    // ---- Slots -----------------------------------------------------------
    bool getSlot(uint8_t slotId, SlotInfo& out) const;
    bool reserveStock(uint8_t slotId);   // stock > 0 ? stock-- : false
    void releaseStock(uint8_t slotId);   // devuelve stock (<= capacidad)
    bool updateSlotPrice(uint8_t slotId, uint32_t newPriceCentavos);
    bool updateSlotStock(uint8_t slotId, uint32_t newStock); // <= capacidad

    // ---- Caja de efectivo ------------------------------------------------
    bool getCoinStock(uint32_t denomCentavos, uint32_t& outStock) const;
    bool addCoins(uint32_t denomCentavos, uint32_t count);
    bool deductCoins(uint32_t denomCentavos, uint32_t count);

    // ---- Administración --------------------------------------------------
    bool verifyAdminPin(const char* pin) const;

    // ---- Última orden física (recuperación Q20) --------------------------
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

    Slot  _slots[VM_CHANNEL_MAX];
    Cash  _cash[4];
    char  _pin[5];         // 4 dígitos ASCII + NUL
    Record _record;

    void seedCache();
    void loadAll();
    void persistAll();
    bool hasMagic() const;
    bool verifyPersist() const;

    void persistSlot(uint8_t index);
    void persistCash(uint8_t index);
    void persistPin();
    void persistRecord();
};

#endif // VM_EEPROM_DATA_H