#ifndef VM_RFID_H
#define VM_RFID_H

#include <Arduino.h>
#include <MFRC522.h>
#include "vm_board_config.h"

// Saldo guardado en Sector 1, Bloque 4 (primeros 4 bytes, uint32_t little-endian)
#define VM_RFID_UID_HEX_MAX 20u

enum class RfidReadResult : uint8_t {
    NONE,
    OK,
    AUTH_FAILED,
    READ_FAILED
};

enum class RfidWriteResult : uint8_t {
    OK,
    AUTH_FAILED,
    WRITE_FAILED
};

class VmRfid {
public:
    VmRfid();

    bool begin();
    RfidReadResult poll();
    RfidWriteResult deductBalance(uint32_t amount);
    RfidWriteResult creditBalance(uint32_t amount);

    bool isAvailable() const { return _available; }
    const char* lastUid() const { return _uidHex; }
    uint32_t lastBalance() const { return _balance; }

private:
    MFRC522  _mfrc;
    bool     _available;
    uint32_t _cooldownUntilMs;

    char     _uidHex[VM_RFID_UID_HEX_MAX + 1];
    uint32_t _balance;

    static const uint8_t BALANCE_BLOCK = 4u;
    MFRC522::MIFARE_Key _key;

    bool reselectCard();
    bool authenticate(uint8_t block);
    bool readBalanceBlock(uint32_t& outBalance);
    bool writeBalanceBlock(uint32_t newBalance);
    void uidToHex();
    void haltCard();
};

#endif // VM_RFID_H
