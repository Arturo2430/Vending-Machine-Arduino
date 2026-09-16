/**
 * @file vm_record_store.cpp
 * @brief Implementación de VmRecordStore (ver vm_record_store.h).
 */

#include "vm_record_store.h"

VmRecordStore::VmRecordStore() {
}

uint16_t VmRecordStore::addrOf(uint8_t fieldOffset) const {
    return (uint16_t)VM_EEPROM_START_ADDR + fieldOffset;
}

bool VmRecordStore::load(Record& out) const {
    if (EEPROM.read(addrOf(0)) != (int)kMagic0 ||
        EEPROM.read(addrOf(1)) != (int)kMagic1) {
        return false;
    }

    uint8_t raw[4];
    for (uint8_t i = 0; i < 4; i++) {
        raw[i] = (uint8_t)EEPROM.read(addrOf(3 + i));
    }
    out.txId  = (uint32_t)raw[0] |
                ((uint32_t)raw[1] << 8) |
                ((uint32_t)raw[2] << 16) |
                ((uint32_t)raw[3] << 24);
    out.channel   = (uint8_t)EEPROM.read(addrOf(7));
    out.result    = (uint8_t)EEPROM.read(addrOf(8));
    out.inProgress = EEPROM.read(addrOf(2)) != 0;
    return true;
}

void VmRecordStore::writeRecord(const Record& rec) {
    EEPROM.update(addrOf(0), kMagic0);
    EEPROM.update(addrOf(1), kMagic1);
    EEPROM.update(addrOf(2), rec.inProgress ? 1 : 0);

    EEPROM.update(addrOf(3), (uint8_t)(rec.txId & 0xFF));
    EEPROM.update(addrOf(4), (uint8_t)((rec.txId >> 8) & 0xFF));
    EEPROM.update(addrOf(5), (uint8_t)((rec.txId >> 16) & 0xFF));
    EEPROM.update(addrOf(6), (uint8_t)((rec.txId >> 24) & 0xFF));

    EEPROM.update(addrOf(7), rec.channel);
    EEPROM.update(addrOf(8), rec.result);
}

void VmRecordStore::beginOrder(uint32_t txId, uint8_t channel) {
    Record rec;
    rec.txId       = txId;
    rec.channel    = channel;
    rec.result     = VM_RESULT_REJECTED_BEFORE_MOTION; // valor por defecto
    rec.inProgress = true;
    writeRecord(rec);
}

void VmRecordStore::completeOrder(uint32_t txId, uint8_t channel, uint8_t result) {
    Record rec;
    rec.txId       = txId;
    rec.channel    = channel;
    rec.result     = result;
    rec.inProgress = false;
    writeRecord(rec);
}

void VmRecordStore::clear() {
    EEPROM.update(addrOf(0), 0x00);
    EEPROM.update(addrOf(1), 0x00);
}