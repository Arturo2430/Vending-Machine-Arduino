/**
 * @file vm_rfid.h
 * @brief Driver no bloqueante para el lector RFID MFRC522 (SPI) con
 *        lectura y escritura de saldo en tarjetas MIFARE Classic.
 *
 * Pinout (SPI hardware del Arduino Mega):
 *   SS/SDA → D53   RST → D8
 *   SCK    → D52   MISO → D50   MOSI → D51
 *   VCC    → 3.3 V   GND → GND
 *
 * Almacenamiento de saldo:
 *   Sector 1, Bloque 4 (primer bloque de datos del sector 1).
 *   Los primeros 4 bytes del bloque contienen el saldo en centavos
 *   codificado en little-endian (uint32_t).
 *
 * @warning NUNCA conectar al riel de 5 V; quema el MFRC522.
 */

#ifndef VM_RFID_H
#define VM_RFID_H

#include <Arduino.h>
#include <MFRC522.h>
#include "vm_board_config.h"

/** Resultado de una operación de lectura de tarjeta. */
enum class RfidReadResult : uint8_t {
    NONE,               ///< No hay tarjeta presente.
    OK,                 ///< Lectura exitosa; saldo disponible.
    AUTH_FAILED,        ///< Fallo de autenticación con la tarjeta.
    READ_FAILED,        ///< Fallo de lectura del bloque de saldo.
};

/** Resultado de una operación de escritura (deducción de saldo). */
enum class RfidWriteResult : uint8_t {
    OK,
    AUTH_FAILED,
    WRITE_FAILED,
};

class VmRfid {
public:
    VmRfid();

    /**
     * @brief Inicializa el bus SPI y el lector.
     * @return true si el MFRC522 respondió correctamente.
     */
    bool begin();

    /**
     * @brief Sondeo no bloqueante: intenta detectar una tarjeta.
     *
     * Si detecta una nueva tarjeta, lee el bloque de saldo y
     * deja los datos accesibles vía lastUid() y lastBalance().
     *
     * @return NONE si no hay tarjeta; OK si se leyó correctamente;
     *         AUTH_FAILED o READ_FAILED en caso de error.
     */
    RfidReadResult poll();

    /**
     * @brief Deduce `amount` centavos del saldo de la tarjeta que
     *        fue leída en el último poll() exitoso.
     *
     * Requiere que la tarjeta siga presente en el campo del lector.
     * Escribe el nuevo saldo al bloque de datos.
     *
     * @param amount Centavos a deducir.
     * @return OK si se escribió correctamente.
     */
    RfidWriteResult deductBalance(uint32_t amount);

    /** true si el lector se inicializó correctamente. */
    bool isAvailable() const { return _available; }

    /** UID hex en mayúsculas de la última tarjeta leída. */
    const char* lastUid() const { return _uidHex; }

    /** Saldo en centavos de la última tarjeta leída. */
    uint32_t lastBalance() const { return _balance; }

private:
    MFRC522  _mfrc;
    bool     _available;
    uint32_t _cooldownUntilMs;

    char     _uidHex[VM_RFID_UID_HEX_MAX + 1]; // UID hex + NUL
    uint32_t _balance;                          // saldo en centavos

    // Sector 1, bloque 4: donde se almacena el saldo.
    static const uint8_t BALANCE_BLOCK = 4u;

    // Clave por defecto de fábrica MIFARE.
    MFRC522::MIFARE_Key _key;

    bool authenticate(uint8_t block);
    bool readBalanceBlock(uint32_t& outBalance);
    bool writeBalanceBlock(uint32_t newBalance);
    void uidToHex();
    void haltCard();
};

#endif // VM_RFID_H
