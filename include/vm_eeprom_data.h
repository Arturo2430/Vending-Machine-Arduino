// Gestor de persistencia en EEPROM
#ifndef VM_EEPROM_DATA_H
#define VM_EEPROM_DATA_H

#include <stdint.h>
#include "vm_types.h"

// Informacion publica de un producto
struct SlotInfo {
    char     productName[17];
    uint32_t priceCentavos;
    uint32_t stock;
    uint32_t capacity;
    bool     enabled;
};

// Interfaz de memoria no volatil
class VmEepromData {
public:
    VmEepromData();

    bool begin(); // Carga o formatea la EEPROM inicial

    // Operaciones sobre inventario de canales
    bool getSlot(uint8_t slotId, SlotInfo& out) const;
    bool reserveStock(uint8_t slotId);
    void releaseStock(uint8_t slotId);

    // Operaciones sobre el flujo de efectivo
    bool getCoinStock(uint32_t denomCentavos, uint32_t& outStock) const;
    bool addCoins(uint32_t denomCentavos, uint32_t count);
    bool deductCoins(uint32_t denomCentavos, uint32_t count);

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

    Slot _slots[VM_CHANNEL_MAX];
    Cash _cash[4];

    void seedCache();
    void loadAll();
    void persistAll();
    bool hasMagic() const;
    bool isCacheValid() const;

    void persistSlot(uint8_t index);
    void persistCash(uint8_t index);
};

#endif // VM_EEPROM_DATA_H