/**
 * @file vm_mega_controller.h
 * @brief Capa de rol esclavo del Arduino Mega sobre el enlace UART VM v2.0.
 *
 * RESPONSABILIDAD DE EDER OMAR ZÚÑIGA ZAVALA (PDF 1):
 *  - UART v2 (extremo Mega): HELLO, ACK, STATUS, SET_MODE, HEARTBEAT,
 *    DISPLAY, VEND, RESULT y rechazo de duplicados.
 *  - Puerta (D3) y barrera óptica (D2) con decodificación y debounce.
 *  - Registro EEPROM del último resultado físico (recuperación Q20).
 *  - SET_MODE con protección BUSY ante dispensado en curso.
 *
 * NO implementa negocio: no autoriza pagos ni interpreta PIN. Solo
 * reenvía teclas (KEY) y copia el display (DISPLAY), según el contrato.
 *
 * Los actuadores (servos, de Diego) y el LCD (de Hugo) se conectan a
 * través de hooks opcionales (dispenser / display sink). Mientras no haya
 * actuador registrado, una orden VEND se confirma y se cierra como
 * REJECTED_BEFORE_MOTION (no hay movimiento posible), manteniendo la
 * semántica completa del mensaje.
 */

#ifndef VM_MEGA_CONTROLLER_H
#define VM_MEGA_CONTROLLER_H

#include <Arduino.h>
#include <MFRC522.h>
#include "vm_uart_protocol.h"
#include "vm_uart_link.h"
#include "vm_record_store.h"

class VmMegaController {
public:
    /**
     * Hooks del actuador de dispensado (los registra el subgrupo de
     * servos, Diego). Todos son opcionales (nullptr si no registrado).
     *  - isBusy:     true mientras haya un dispensado físico en curso.
     *  - start:      arranca el ciclo del canal; false si no puede mover.
     *  - poll:       avanza el ciclo no bloqueante (llamar cada loop()).
     *  - consumeResult: resultado del ciclo terminado; -1 = ninguno,
     *                  0 = DELIVERED, 2 = UNCERTAIN.
     */
    struct DispenserHooks {
        void (*stop)(void);
        bool (*isBusy)(void);
        bool (*startDispense)(uint8_t channel);
        void (*poll)(void);
        int  (*consumeResult)(void);
    };

    /** Destino del texto DISPLAY (LCD 20x4, de Hugo). Opcional. */
    typedef void (*DisplaySink)(const char* line1, const char* line2,
                                const char* line3, const char* line4);

    VmMegaController();

    /**
     * Conecta el controlador al enlace UART y al registro EEPROM.
     * `link` debe tener begin() ya llamada. Deja el sistema en
     * MODE_MANTENIMIENTO (UART-REQ-003), recupera el último registro y,
     * si quedó "en progreso" por un corte, lo cierra como UNCERTAIN y lo
     * notifica al ESP32 por RESULT (recuperación Q20).
     */
    void begin(VmUartLink& link, VmRecordStore& store);

    /** Registra los hooks del actuador (módulo de servos de Diego). */
    void setDispenser(const DispenserHooks& hooks);

    /** Registra el destino del display (módulo LCD de Hugo). */
    void setDisplaySink(DisplaySink sink);

    /**
     * Debe llamarse continuamente desde loop(): consume bytes del enlace,
     * actualiza sensores, avanza el ciclo no bloqueante del actuador y
     * reporta RESULT cuando una orden concluye.
     */
    void poll();

    /** Envía HELLO identificándose como ROLE_MEGA (para arranque). */
    void sendHelloHandshake();

    /** Inicializa el lector RC522 conectado al SPI hardware del Mega. */
    void beginRfid();

    /**
     * Envía el mensaje KEY de una tecla estable con su contador local
     * (key_seq). Lo dispara el subgrupo de teclado de Hugo cuando haya
     * un evento físico.
     */
    void sendKeyEvent(char keyAscii);

    /** Modo operativo vigente (VM_MODE_VENTA o VM_MODE_MANTENIMIENTO). */
    uint8_t getMode() const;

    /** true si la puerta está registrada como cerrada (con debounce). */
    bool isDoorClosed() const;

    /** Lectura directa de la barrera óptica (0 libre, 1 ocupada). */
    bool isBarrierOccupied() const;

    /** Transacción en ejecución, o VM_TX_ID_NONE si inactivo. */
    uint32_t getCurrentTransactionId() const;

private:
    VmUartLink* _link;
    VmRecordStore* _store;

    DispenserHooks _dispenser;
    DisplaySink _displaySink;

    uint8_t _mode;                 // vm_mode_t
    uint8_t _keySeq;               // contador local de teclas (módulo 256)

    VmRecordStore::Record _record; // último registro físico (EEPROM)

    bool _orderInProgress;
    uint32_t _orderTx;
    uint8_t _orderChannel;

    // Debounce de puerta
    bool _doorSampled;
    bool _doorStable;
    uint32_t _doorLastChangeMs;

    void handleFrame(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint8_t len);
    static VmMegaController* s_instance;
    static void frameTrampoline(uint8_t cmd, uint8_t seq,
                                const uint8_t* payload, uint8_t len);

    void handleHello(uint8_t seq);
    void handleAck(const uint8_t* payload, uint8_t len);
    void handleStatus(uint8_t seq, const uint8_t* payload, uint8_t len);
    void handleSetMode(uint8_t seq, const uint8_t* payload, uint8_t len);
    void handleHeartbeat(uint8_t seq, const uint8_t* payload, uint8_t len);
    void handleDisplay(uint8_t seq, const uint8_t* payload, uint8_t len);
    void handleVend(uint8_t seq, const uint8_t* payload, uint8_t len);
    void handleTolerated(uint8_t seq, const uint8_t* payload, uint8_t len,
                         uint8_t expectLen, uint8_t cmdRef);

    void ack(uint8_t seq, uint8_t cmdRef, uint8_t resultado, uint8_t motivo);
    void sendResult(uint32_t txId, uint8_t result);

    void reconcileOnBoot();
    void acceptOrder(uint32_t txId, uint8_t channel);
    void updateOrder();
    void updateDoor();
    void refreshRecord();

    bool dispenserBusy() const;
    void pollRfid();
    bool isRfidHex(uint8_t value) const;

    MFRC522 _rfid;
    bool _rfidAwaitingAck;
    uint32_t _rfidAckStartedMs;
    uint32_t _rfidCooldownUntilMs;
    uint8_t _rfidUid[VM_RFID_UID_MAX];
    uint8_t _rfidUidLen;
};

#endif // VM_MEGA_CONTROLLER_H