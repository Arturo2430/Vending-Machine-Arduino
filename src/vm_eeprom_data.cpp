/**
 * @file vm_eeprom_data.cpp
 * @brief Persistencia en EEPROM para la máquina expendedora.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "vm_eeprom_data.h"
#include "vm_board_config.h"

// ---------------------------------------------------------------------------
// Layout en EEPROM (offsets desde VM_EEPROM_START_ADDR)
//
//  0     : MAGIC0 ('S')
//  1     : MAGIC1 ('A')
//  2     : VERSION (3)
//  3..22 : Caja de efectivo: 4 × (denom u32 + qty u8) = 20 bytes
//  23..114: Slots: 4 × (name[16] + price u32 + stock u8 + cap u8 + ena u8) = 23 bytes c/u
//           Total: 23 × 4 = 92 bytes
// ---------------------------------------------------------------------------
enum {
    OFF_MAGIC0  = 0,
    OFF_MAGIC1  = 1,
    OFF_VERSION = 2,
    OFF_CASH    = 3,   // 20 bytes  (3..22)
    OFF_SLOTS   = 23   // 92 bytes  (23..114)
};

static const uint8_t MAGIC0  = 'S';
static const uint8_t MAGIC1  = 'A';
static const uint8_t VERSION = 3u;  // versión 3: sin PIN, sin registro transaccional

// ---------------------------------------------------------------------------
// Semilla de productos (guardada en Flash para ahorrar RAM)
// ---------------------------------------------------------------------------
typedef struct {
    char     name[16];
    uint32_t priceCentavos;
    uint8_t  stock;
    uint8_t  capacity;
} SeedSlot;

static const SeedSlot SEED_SLOTS[VM_CHANNEL_MAX] PROGMEM = {
    { "Coca-Cola 355ml", 1800u, 8u, 10u },
    { "Galletas Marias", 1500u, 6u, 10u },
    { "Agua 600ml",      1200u, 9u, 10u },
    { "Jugo Naranja",    1400u, 5u, 10u },
};

// Denominaciones de moneda: $1, $2, $5, $10 (en centavos)
static const uint32_t SEED_CASH_DENOMS[4] PROGMEM = { 100u, 200u, 500u, 1000u };

// ---------------------------------------------------------------------------
// Funciones auxiliares de EEPROM
// ---------------------------------------------------------------------------
static uint16_t ea(uint8_t off) {
    return (uint16_t)VM_EEPROM_START_ADDR + (uint16_t)off;
}
static void ewrite(uint8_t off, uint8_t v) { EEPROM.update(ea(off), v); }
static uint8_t eread(uint8_t off)           { return EEPROM.read(ea(off)); }

static void ewrite32(uint8_t off, uint32_t v) {
    ewrite(off,     (uint8_t)(v         & 0xFFu));
    ewrite(off + 1, (uint8_t)((v >>  8) & 0xFFu));
    ewrite(off + 2, (uint8_t)((v >> 16) & 0xFFu));
    ewrite(off + 3, (uint8_t)((v >> 24) & 0xFFu));
}

static uint32_t eread32(uint8_t off) {
    return (uint32_t)eread(off)
         | ((uint32_t)eread(off + 1) <<  8)
         | ((uint32_t)eread(off + 2) << 16)
         | ((uint32_t)eread(off + 3) << 24);
}

// ---------------------------------------------------------------------------
// VmEepromData
// ---------------------------------------------------------------------------
VmEepromData::VmEepromData() {
    memset(_slots, 0, sizeof(_slots));
    memset(_cash,  0, sizeof(_cash));
}

void VmEepromData::seedCache() {
    for (uint8_t i = 0; i < VM_CHANNEL_MAX; i++) {
        SeedSlot tmp;
        memcpy_P(&tmp, &SEED_SLOTS[i], sizeof(tmp));
        memset(_slots[i].name, 0, sizeof(_slots[i].name));
        memcpy(_slots[i].name, tmp.name, 16);
        _slots[i].name[16] = '\0';
        _slots[i].price    = tmp.priceCentavos;
        _slots[i].stock    = tmp.stock;
        _slots[i].cap      = tmp.capacity;
        _slots[i].enabled  = 1u;
    }

    for (uint8_t i = 0; i < 4; i++) {
        uint32_t denom;
        memcpy_P(&denom, &SEED_CASH_DENOMS[i], sizeof(denom));
        _cash[i].denom = denom;
        _cash[i].qty   = VM_SEED_CASH_DEFAULT_QTY;
    }
}

bool VmEepromData::hasMagic() const {
    return eread(OFF_MAGIC0) == MAGIC0 &&
           eread(OFF_MAGIC1) == MAGIC1 &&
           eread(OFF_VERSION) == VERSION;
}

void VmEepromData::loadAll() {
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t base    = OFF_CASH + i * 5u;
        _cash[i].denom  = eread32(base);
        _cash[i].qty    = eread(base + 4);
    }

    for (uint8_t i = 0; i < VM_CHANNEL_MAX; i++) {
        uint8_t base = OFF_SLOTS + i * 23u;
        memset(_slots[i].name, 0, sizeof(_slots[i].name));
        for (uint8_t j = 0; j < 16; j++) {
            _slots[i].name[j] = (char)eread(base + j);
        }
        _slots[i].name[16] = '\0';
        _slots[i].price    = eread32(base + 16u);
        _slots[i].stock    = eread(base + 20u);
        _slots[i].cap      = eread(base + 21u);
        _slots[i].enabled  = eread(base + 22u);
    }
}

void VmEepromData::persistAll() {
    ewrite(OFF_MAGIC0,  MAGIC0);
    ewrite(OFF_MAGIC1,  MAGIC1);
    ewrite(OFF_VERSION, VERSION);
    for (uint8_t i = 0; i < 4; i++) {
        persistCash(i);
    }
    for (uint8_t i = 0; i < VM_CHANNEL_MAX; i++) {
        persistSlot(i);
    }
}

void VmEepromData::persistSlot(uint8_t index) {
    if (index >= VM_CHANNEL_MAX) return;
    uint8_t base = OFF_SLOTS + index * 23u;
    for (uint8_t j = 0; j < 16; j++) {
        ewrite(base + j, (uint8_t)_slots[index].name[j]);
    }
    ewrite32(base + 16u, _slots[index].price);
    ewrite(base + 20u,   _slots[index].stock);
    ewrite(base + 21u,   _slots[index].cap);
    ewrite(base + 22u,   _slots[index].enabled);
}

void VmEepromData::persistCash(uint8_t index) {
    if (index >= 4) return;
    uint8_t base = OFF_CASH + index * 5u;
    ewrite32(base,   _cash[index].denom);
    ewrite(base + 4, _cash[index].qty);
}

bool VmEepromData::begin() {
    if (hasMagic()) {
        loadAll();
        return true;
    }
    // Primera ejecución o versión distinta: inicializar con datos semilla
    seedCache();
    persistAll();
    return hasMagic();
}

// ---------------------------------------------------------------------------
// Slots de producto
// ---------------------------------------------------------------------------
bool VmEepromData::getSlot(uint8_t slotId, SlotInfo& out) const {
    if (slotId < VM_CHANNEL_MIN || slotId > VM_CHANNEL_MAX) return false;
    const Slot& s = _slots[slotId - 1u];
    memcpy(out.productName, s.name, sizeof(out.productName));
    out.productName[sizeof(out.productName) - 1] = '\0';
    out.priceCentavos = s.price;
    out.stock         = s.stock;
    out.capacity      = s.cap;
    out.enabled       = s.enabled != 0u;
    return true;
}

bool VmEepromData::reserveStock(uint8_t slotId) {
    if (slotId < VM_CHANNEL_MIN || slotId > VM_CHANNEL_MAX) return false;
    Slot& s = _slots[slotId - 1u];
    if (s.enabled == 0u || s.stock == 0u) return false;
    s.stock--;
    persistSlot(slotId - 1u);
    return true;
}

void VmEepromData::releaseStock(uint8_t slotId) {
    if (slotId < VM_CHANNEL_MIN || slotId > VM_CHANNEL_MAX) return;
    Slot& s = _slots[slotId - 1u];
    if (s.stock < s.cap) {
        s.stock++;
        persistSlot(slotId - 1u);
    }
}

// ---------------------------------------------------------------------------
// Caja de efectivo
// ---------------------------------------------------------------------------
bool VmEepromData::getCoinStock(uint32_t denomCentavos, uint32_t& outStock) const {
    for (uint8_t i = 0; i < 4; i++) {
        if (_cash[i].denom == denomCentavos) {
            outStock = _cash[i].qty;
            return true;
        }
    }
    return false;
}

bool VmEepromData::addCoins(uint32_t denomCentavos, uint32_t count) {
    for (uint8_t i = 0; i < 4; i++) {
        if (_cash[i].denom == denomCentavos) {
            uint32_t qty = (uint32_t)_cash[i].qty + count;
            if (qty > 250u) qty = 250u; // tope de seguridad
            _cash[i].qty = (uint8_t)qty;
            persistCash(i);
            return true;
        }
    }
    return false;
}

bool VmEepromData::deductCoins(uint32_t denomCentavos, uint32_t count) {
    for (uint8_t i = 0; i < 4; i++) {
        if (_cash[i].denom == denomCentavos) {
            if (count > (uint32_t)_cash[i].qty) return false;
            _cash[i].qty -= (uint8_t)count;
            persistCash(i);
            return true;
        }
    }
    return false;
}