/**
 * @file vm_mega_controller.cpp
 * @brief Implementación de VmMegaController (ver vm_mega_controller.h).
 */

#include "vm_mega_controller.h"

VmMegaController* VmMegaController::s_instance = nullptr;

VmMegaController::VmMegaController()
    : _link(nullptr),
      _store(nullptr),
      _dispenser{nullptr, nullptr, nullptr, nullptr},
      _displaySink(nullptr),
      _mode(VM_MODE_MANTENIMIENTO),   // UART-REQ-003: arranque en mantenimiento
      _keySeq(0),
      _orderInProgress(false),
      _orderTx(VM_TX_ID_NONE),
      _orderChannel(0),
      _doorSampled(false),
      _doorStable(false),
      _doorLastChangeMs(0) {
    _record.txId = VM_TX_ID_NONE;
    _record.channel = 0;
    _record.result = VM_RESULT_REJECTED_BEFORE_MOTION;
    _record.inProgress = false;
}

void VmMegaController::begin(VmUartLink& link, VmRecordStore& store) {
    _link = &link;
    _store = &store;
    s_instance = this;

    _link->onFrame(&VmMegaController::frameTrampoline);

    // Recupera el último registro físico válido antes de aceptar órdenes
    // (el registro se conserva para idempotencia y reconciliación).
    _store->load(_record);

    // Si quedó una orden "en progreso" (corte de energía a mitad de giro),
    // se cierra como UNCERTAIN y se notifica al ESP32 para reconciliación.
    reconcileOnBoot();
}

void VmMegaController::setDispenser(const DispenserHooks& hooks) {
    _dispenser = hooks;
}

void VmMegaController::setDisplaySink(DisplaySink sink) {
    _displaySink = sink;
}

void VmMegaController::poll() {
    if (_link == nullptr) {
        return;
    }
    _link->poll();
    updateDoor();
    updateOrder();
}

void VmMegaController::sendHelloHandshake() {
    if (_link != nullptr) {
        _link->sendHello(_link->nextSeq(), VM_ROLE_MEGA);
    }
}

void VmMegaController::sendKeyEvent(char keyAscii) {
    if (_link == nullptr) {
        return;
    }
    uint8_t seq = _link->nextSeq();
    uint8_t keySeq = _keySeq;
    _keySeq = (uint8_t)(_keySeq + 1);  // contador local módulo 256
    _link->sendKey(seq, (uint8_t)keyAscii, keySeq);
}

uint8_t VmMegaController::getMode() const {
    return _mode;
}

bool VmMegaController::isDoorClosed() const {
    return _doorStable;
}

bool VmMegaController::isBarrierOccupied() const {
    return digitalRead(VM_PIN_BARRIER) == VM_BARRIER_OCCUPIED_LEVEL;
}

uint32_t VmMegaController::getCurrentTransactionId() const {
    return _orderInProgress ? _orderTx : VM_TX_ID_NONE;
}

// ------------------------------------------------------------
// Parser de tramas entrantes
// ------------------------------------------------------------

void VmMegaController::frameTrampoline(uint8_t cmd, uint8_t seq,
                                       const uint8_t* payload, uint8_t len) {
    if (s_instance != nullptr) {
        s_instance->handleFrame(cmd, seq, payload, len);
    }
}

void VmMegaController::handleFrame(uint8_t cmd, uint8_t seq,
                                   const uint8_t* payload, uint8_t len) {
    switch (cmd) {
        case VM_CMD_HELLO:     handleHello(seq);                    break;
        case VM_CMD_ACK:       /* UART-REQ-005: nunca responde */   break;
        case VM_CMD_STATUS:    handleStatus(seq, payload, len);     break;
        case VM_CMD_SET_MODE:  handleSetMode(seq, payload, len);    break;
        case VM_CMD_HEARTBEAT: handleHeartbeat(seq, payload, len);  break;
        case VM_CMD_KEY:       handleTolerated(seq, payload, len, VM_LEN_KEY, VM_CMD_KEY);    break;
        case VM_CMD_DISPLAY:   handleDisplay(seq, payload, len);    break;
        case VM_CMD_VEND:      handleVend(seq, payload, len);       break;
        case VM_CMD_RESULT:    handleTolerated(seq, payload, len, VM_LEN_RESULT, VM_CMD_RESULT); break;
        default:
            ack(seq, cmd, VM_ACK_REJECTED, VM_REASON_INVALID_CMD);
            break;
    }
}

void VmMegaController::handleHello(uint8_t seq) {
    // UART-REQ-004: HELLO se responde de inmediato con HELLO mismo SEQ.
    if (_link != nullptr) {
        _link->sendHello(seq, VM_ROLE_MEGA);
    }
}

void VmMegaController::handleStatus(uint8_t seq, const uint8_t* payload, uint8_t len) {
    if (len != VM_LEN_STATUS_REQ) {
        // El Mega solo recibe la solicitud (LEN=0).
        ack(seq, VM_CMD_STATUS, VM_ACK_REJECTED, VM_REASON_INVALID_LENGTH);
        return;
    }
    if (_link == nullptr) {
        return;
    }
    uint8_t door = isDoorClosed() ? VM_DOOR_CLOSED : VM_DOOR_OPEN;
    uint8_t barrier = isBarrierOccupied() ? VM_BARRIER_OCCUPIED : VM_BARRIER_FREE;
    _link->sendStatusResponse(seq, door, barrier, _mode, getCurrentTransactionId());
}

void VmMegaController::handleSetMode(uint8_t seq, const uint8_t* payload, uint8_t len) {
    if (len != VM_LEN_SET_MODE) {
        ack(seq, VM_CMD_SET_MODE, VM_ACK_REJECTED, VM_REASON_INVALID_LENGTH);
        return;
    }
    // UART-REQ-007: si hay dispensado en curso no se cambia de modo (BUSY).
    if (dispenserBusy() || _orderInProgress) {
        ack(seq, VM_CMD_SET_MODE, VM_ACK_REJECTED, VM_REASON_BUSY);
        return;
    }
    uint8_t mode = (payload != nullptr) ? payload[0] : 0xFF;
    if (mode != VM_MODE_VENTA && mode != VM_MODE_MANTENIMIENTO) {
        ack(seq, VM_CMD_SET_MODE, VM_ACK_REJECTED, VM_REASON_INVALID_PAYLOAD);
        return;
    }
    _mode = mode;
    ack(seq, VM_CMD_SET_MODE, VM_ACK_RECEIVED, VM_REASON_NONE);
}

void VmMegaController::handleHeartbeat(uint8_t seq, const uint8_t* payload, uint8_t len) {
    if (len != VM_LEN_HEARTBEAT) {
        ack(seq, VM_CMD_HEARTBEAT, VM_ACK_REJECTED, VM_REASON_INVALID_LENGTH);
        return;
    }
    // Modo eco: responde HEARTBEAT con el mismo SEQ.
    if (_link != nullptr) {
        _link->sendHeartbeat(seq);
    }
}

void VmMegaController::handleDisplay(uint8_t seq, const uint8_t* payload, uint8_t len) {
    if (len != VM_LEN_DISPLAY || payload == nullptr) {
        ack(seq, VM_CMD_DISPLAY, VM_ACK_REJECTED, VM_REASON_INVALID_LENGTH);
        return;
    }
    // UART-REQ-009: copia literal sin interpretar. Nota: `payload` apunta
    // al buffer transitorio del parser; el sink debe copiar al instante.
    if (_displaySink != nullptr) {
        _displaySink((const char*)payload, (const char*)(payload + VM_DISPLAY_LINE_LEN));
    }
    ack(seq, VM_CMD_DISPLAY, VM_ACK_RECEIVED, VM_REASON_NONE);
}

void VmMegaController::handleVend(uint8_t seq, const uint8_t* payload, uint8_t len) {
    if (len != VM_LEN_VEND || payload == nullptr) {
        ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_INVALID_LENGTH);
        return;
    }

    uint8_t channel = payload[0];
    uint32_t txId = VmUartLink::decodeU32LE(&payload[1]);

    // ------------------------------------------------------------
    // Idempotencia: mismo transaction_id (UART-REQ-010 / UART-REQ-011)
    // Nota: se protege la ÚLTIMA transacción guardada en EEPROM. El ESP32
    // genera tx monotónicos con una sola orden activa a la vez, por lo que
    // un duplicado siempre corresponde a la última orden procesada.
    // ------------------------------------------------------------
    if (_record.txId == txId &&
        _record.txId != VM_TX_ID_NONE && _record.txId != VM_TX_ID_NULL) {
        if (_record.channel == channel) {
            // Nunca girar de nuevo; ACK y, si ya concluyó, reenviar resultado.
            ack(seq, VM_CMD_VEND, VM_ACK_RECEIVED, VM_REASON_NONE);
            if (!_record.inProgress && _orderTx != txId) {
                sendResult(_record.txId, _record.result);
            }
        } else {
            ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_DUPLICATE_CONFLICT);
        }
        return;
    }

    // ------------------------------------------------------------
    // Validaciones de una orden nueva
    // Orden de validación siguiendo §6.8 (modo primero). Nota: la prueba
    // UART-T03 (canal fuera de rango -> INVALID_CHANNEL) se ejecuta en
    // modo VENTA para llegar a la comprobación de canal.
    // ------------------------------------------------------------
    if (_mode != VM_MODE_VENTA) {
        ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_INVALID_STATE);
        return;
    }
    if (channel < VM_CHANNEL_MIN || channel > VM_CHANNEL_MAX) {
        ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_INVALID_CHANNEL);
        return;
    }
    if (dispenserBusy() || _orderInProgress) {
        ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_BUSY);
        return;
    }
    if (!isDoorClosed()) {
        ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_UNSAFE_PHYSICAL_STATE);
        return;
    }
    if (txId == VM_TX_ID_NULL || txId == VM_TX_ID_NONE) {
        ack(seq, VM_CMD_VEND, VM_ACK_REJECTED, VM_REASON_INVALID_PAYLOAD);
        return;
    }

    // Orden aceptada: confirmar para luego reportar el resultado asíncrono.
    ack(seq, VM_CMD_VEND, VM_ACK_RECEIVED, VM_REASON_NONE);
    acceptOrder(txId, channel);
}

void VmMegaController::handleTolerated(uint8_t seq, const uint8_t* payload,
                                       uint8_t len, uint8_t expectLen,
                                       uint8_t cmdRef) {
    // KEY y RESULT son tramas del Mega; si llegan (retransmisión o ruido),
    // se toleran con forma válida o se rechazan con INVALID_LENGTH.
    if (len != expectLen || payload == nullptr) {
        ack(seq, cmdRef, VM_ACK_REJECTED, VM_REASON_INVALID_LENGTH);
        return;
    }
    ack(seq, cmdRef, VM_ACK_RECEIVED, VM_REASON_NONE);
}

// ------------------------------------------------------------
// Auxiliares
// ------------------------------------------------------------

void VmMegaController::ack(uint8_t seq, uint8_t cmdRef,
                           uint8_t resultado, uint8_t motivo) {
    if (_link != nullptr) {
        _link->sendAck(seq, cmdRef, resultado, motivo);
    }
}

void VmMegaController::sendResult(uint32_t txId, uint8_t result) {
    if (_link != nullptr) {
        _link->sendResult(_link->nextSeq(), txId, result);
    }
}

bool VmMegaController::dispenserBusy() const {
    return _dispenser.isBusy != nullptr && _dispenser.isBusy();
}

void VmMegaController::reconcileOnBoot() {
    if (_store == nullptr || _link == nullptr) {
        return;
    }
    if (_record.inProgress) {
        _store->completeOrder(_record.txId, _record.channel, VM_RESULT_UNCERTAIN);
        _record.inProgress = false;
        _record.result = VM_RESULT_UNCERTAIN;
        // Best-effort: si el ESP32 aún no escucha, esta RESULT se pierde,
        // pero la reconciliación se completa cuando retransmita el VEND
        // (el Mega reenvía el resultado guardado por idempotencia).
        sendResult(_record.txId, VM_RESULT_UNCERTAIN);
    }
}

void VmMegaController::acceptOrder(uint32_t txId, uint8_t channel) {
    _orderInProgress = true;
    _orderTx = txId;
    _orderChannel = channel;

    _store->beginOrder(txId, channel);
    refreshRecord();

    if (isBarrierOccupied()) {
        // Barrera obstruida: rechazo antes de activar actuadores.
        _store->completeOrder(txId, channel, VM_RESULT_REJECTED_BEFORE_MOTION);
        refreshRecord();
        sendResult(txId, VM_RESULT_REJECTED_BEFORE_MOTION);
        _orderInProgress = false;
        _orderTx = VM_TX_ID_NONE;
        _orderChannel = 0;
        return;
    }

    // Sin actuador registrado (o si no puede mover): no hay giro posible,
    // se cierra la orden como REJECTED_BEFORE_MOTION. El subgrupo de
    // servos (Diego) conectará su modulo vía setDispenser().
    bool started = (_dispenser.startDispense != nullptr) &&
                   _dispenser.startDispense(channel);
    if (!started) {
        _store->completeOrder(txId, channel, VM_RESULT_REJECTED_BEFORE_MOTION);
        refreshRecord();
        sendResult(txId, VM_RESULT_REJECTED_BEFORE_MOTION);
        _orderInProgress = false;
        _orderTx = VM_TX_ID_NONE;
        _orderChannel = 0;
    }
}

void VmMegaController::updateOrder() {
    if (_dispenser.poll != nullptr) {
        _dispenser.poll();  // avanza el ciclo no bloqueante del actuador
    }

    if (!_orderInProgress) {
        return;
    }

    if (_dispenser.consumeResult == nullptr) {
        return;
    }
    int r = _dispenser.consumeResult();
    if (r < 0) {
        return;  // ciclo aún en curso
    }

    uint8_t resultado = (r == 0) ? VM_RESULT_DELIVERED : VM_RESULT_UNCERTAIN;

    _store->completeOrder(_orderTx, _orderChannel, resultado);
    refreshRecord();
    sendResult(_orderTx, resultado);

    _orderInProgress = false;
    _orderTx = VM_TX_ID_NONE;
    _orderChannel = 0;
}

void VmMegaController::updateDoor() {
    bool raw = digitalRead(VM_PIN_DOOR) == VM_DOOR_CLOSED_LEVEL;
    if (raw != _doorSampled) {
        _doorSampled = raw;
        _doorLastChangeMs = millis();
        return;
    }
    if ((uint32_t)(millis() - _doorLastChangeMs) >= VM_DOOR_DEBOUNCE_MS) {
        _doorStable = raw;
    }
}

void VmMegaController::refreshRecord() {
    VmRecordStore::Record rec;
    if (_store->load(rec)) {
        _record = rec;
    }
}