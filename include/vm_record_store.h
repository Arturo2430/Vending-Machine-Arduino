/**
 * @file vm_record_store.h
 * @brief Persistencia en EEPROM del último resultado físico de dispensado.
 *
 * Responsabilidad (contra documento de Arquitectura, Eder Zuñiga):
 *  - Conservar el último resultado físico válido para reconciliación
 *    tras un corte de energía (Q20) o reinicio del Mega.
 *  - Permitir la idempotencia ante VEND duplicado (mismo transaction_id)
 *    incluso si el ESP32 retransmite después de un corte (Q18).
 *
 * Un registro se escribe "en progreso" al aceptar una orden y se cierra
 * con el resultado (DELIVERED / REJECTED_BEFORE_MOTION / UNCERTAIN) al
 * finalizar. El registro permanece válido después de cerrarse para
 * responder con el resultado previo ante duplicados.
 *
 * Layout (a partir de VM_EEPROM_START_ADDR, 9 bytes):
 *   [0] magic 'V'  [1] magic 'M'  [2] inProgress
 *   [3..6] transaction_id (little-endian)  [7] channel  [8] result
 */

#ifndef VM_RECORD_STORE_H
#define VM_RECORD_STORE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "vm_uart_protocol.h"
#include "vm_board_config.h"

class VmRecordStore {
public:
    struct Record {
        uint32_t txId;
        uint8_t  channel;
        uint8_t  result;      // vm_dispense_result_t
        bool     inProgress;
    };

    VmRecordStore();

    /**
     * Lee el registro persistido. Devuelve true si existe un registro
     * válido (magic correcto) y rellena `out`; false si está vacío o
     * corrupto. Llamar una vez en setup() para recuperar el último
     * estado físico antes de aceptar órdenes.
     */
    bool load(Record& out) const;

    /** Escribe el registro marcado como "en progreso" (VEND aceptado). */
    void beginOrder(uint32_t txId, uint8_t channel);

    /**
     * Cierra el registro con el resultado físico final. El cierre no
     * invalida el registro: queda disponible para responder duplicados.
     */
    void completeOrder(uint32_t txId, uint8_t channel, uint8_t result);

    /** Invalida el registro (reconciliación / limpieza de mantenimiento). */
    void clear();

private:
    static const uint8_t kMagic0 = 0x56u; // 'V'
    static const uint8_t kMagic1 = 0x4Du; // 'M'

    uint16_t addrOf(uint8_t fieldOffset) const;
    void writeRecord(const Record& rec);
};

#endif // VM_RECORD_STORE_H