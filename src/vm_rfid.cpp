/**
 * @file vm_rfid.cpp
 * @brief Implementación del driver RFID no bloqueante con lectura/escritura
 *        de saldo en tarjetas MIFARE Classic (sector 1, bloque 4).
 */

#include "vm_rfid.h"
#include <SPI.h>
#include <string.h>

VmRfid::VmRfid()
    : _mfrc(VM_PIN_RFID_SS, VM_PIN_RFID_RST),
      _available(false),
      _cooldownUntilMs(0u),
      _balance(0u) {
    memset(_uidHex, 0, sizeof(_uidHex));
    // Clave por defecto de fábrica: FF FF FF FF FF FF
    for (uint8_t i = 0; i < 6; i++) {
        _key.keyByte[i] = 0xFF;
    }
}

bool VmRfid::begin() {
    SPI.begin();
    _mfrc.PCD_Init();

    // Verificar que el módulo responde.
    byte version = _mfrc.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.println(F("[RFID] MFRC522 no detectado. Verificar cableado SPI."));
        _available = false;
        return false;
    }

    Serial.print(F("[RFID] MFRC522 listo. Version=0x"));
    Serial.println(version, HEX);
    _available = true;
    return true;
}

RfidReadResult VmRfid::poll() {
    if (!_available) return RfidReadResult::NONE;

    // Cooldown entre lecturas sucesivas.
    if ((int32_t)(millis() - _cooldownUntilMs) < 0) {
        return RfidReadResult::NONE;
    }

    // Detección no bloqueante.
    if (!_mfrc.PICC_IsNewCardPresent()) return RfidReadResult::NONE;
    if (!_mfrc.PICC_ReadCardSerial())   return RfidReadResult::NONE;

    // Convertir UID a hex.
    uidToHex();
    Serial.print(F("[RFID] Tarjeta detectada: "));
    Serial.println(_uidHex);

    // Autenticar y leer saldo.
    if (!authenticate(BALANCE_BLOCK)) {
        Serial.println(F("[RFID] Error de autenticacion."));
        haltCard();
        _cooldownUntilMs = millis() + VM_RFID_COOLDOWN_MS;
        return RfidReadResult::AUTH_FAILED;
    }

    if (!readBalanceBlock(_balance)) {
        Serial.println(F("[RFID] Error de lectura del bloque de saldo."));
        haltCard();
        _cooldownUntilMs = millis() + VM_RFID_COOLDOWN_MS;
        return RfidReadResult::READ_FAILED;
    }

    Serial.print(F("[RFID] Saldo leido: $"));
    Serial.print(_balance / 100u);
    Serial.print('.');
    if ((_balance % 100u) < 10u) Serial.print('0');
    Serial.println(_balance % 100u);

    // NO hacemos halt aquí: la tarjeta debe seguir presente para
    // una posible escritura posterior (deductBalance).
    return RfidReadResult::OK;
}

RfidWriteResult VmRfid::deductBalance(uint32_t amount) {
    if (amount > _balance) {
        return RfidWriteResult::WRITE_FAILED;
    }

    uint32_t newBalance = _balance - amount;

    // Re-autenticar (la autenticación se pierde tras cierto tiempo).
    if (!authenticate(BALANCE_BLOCK)) {
        haltCard();
        _cooldownUntilMs = millis() + VM_RFID_COOLDOWN_MS;
        return RfidWriteResult::AUTH_FAILED;
    }

    if (!writeBalanceBlock(newBalance)) {
        haltCard();
        _cooldownUntilMs = millis() + VM_RFID_COOLDOWN_MS;
        return RfidWriteResult::WRITE_FAILED;
    }

    _balance = newBalance;

    Serial.print(F("[RFID] Nuevo saldo escrito: $"));
    Serial.print(newBalance / 100u);
    Serial.print('.');
    if ((newBalance % 100u) < 10u) Serial.print('0');
    Serial.println(newBalance % 100u);

    haltCard();
    _cooldownUntilMs = millis() + VM_RFID_COOLDOWN_MS;
    return RfidWriteResult::OK;
}

// ---------------------------------------------------------------------------
// Helpers internos
// ---------------------------------------------------------------------------

bool VmRfid::authenticate(uint8_t block) {
    MFRC522::StatusCode status = _mfrc.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &_key, &(_mfrc.uid));
    return (status == MFRC522::STATUS_OK);
}

bool VmRfid::readBalanceBlock(uint32_t& outBalance) {
    uint8_t buffer[18]; // 16 datos + 2 CRC
    uint8_t size = sizeof(buffer);

    MFRC522::StatusCode status = _mfrc.MIFARE_Read(BALANCE_BLOCK, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        return false;
    }

    // Decodificar saldo little-endian desde los primeros 4 bytes.
    outBalance = (uint32_t)buffer[0]
               | ((uint32_t)buffer[1] << 8)
               | ((uint32_t)buffer[2] << 16)
               | ((uint32_t)buffer[3] << 24);
    return true;
}

bool VmRfid::writeBalanceBlock(uint32_t newBalance) {
    uint8_t buffer[16];
    memset(buffer, 0, sizeof(buffer));

    // Codificar saldo little-endian en los primeros 4 bytes.
    buffer[0] = (uint8_t)(newBalance & 0xFF);
    buffer[1] = (uint8_t)((newBalance >> 8) & 0xFF);
    buffer[2] = (uint8_t)((newBalance >> 16) & 0xFF);
    buffer[3] = (uint8_t)((newBalance >> 24) & 0xFF);

    MFRC522::StatusCode status = _mfrc.MIFARE_Write(BALANCE_BLOCK, buffer, 16);
    return (status == MFRC522::STATUS_OK);
}

void VmRfid::uidToHex() {
    uint8_t len = _mfrc.uid.size;
    if (len > (VM_RFID_UID_HEX_MAX / 2u)) {
        len = VM_RFID_UID_HEX_MAX / 2u;
    }
    for (uint8_t i = 0; i < len; i++) {
        uint8_t val = _mfrc.uid.uidByte[i];
        uint8_t hi = (uint8_t)(val >> 4);
        uint8_t lo = (uint8_t)(val & 0x0F);
        _uidHex[2u * i]       = (hi < 10u) ? ('0' + hi) : ('A' + hi - 10u);
        _uidHex[2u * i + 1u]  = (lo < 10u) ? ('0' + lo) : ('A' + lo - 10u);
    }
    _uidHex[len * 2u] = '\0';
}

void VmRfid::haltCard() {
    _mfrc.PICC_HaltA();
    _mfrc.PCD_StopCrypto1();
}
