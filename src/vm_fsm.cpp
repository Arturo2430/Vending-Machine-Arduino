/**
 * @file vm_fsm.cpp
 * @brief Implementación de la máquina de estados (portada del ESP32,
 *        con pago RFID MIFARE y efectivo).
 */

#include <string.h>
#include "vm_fsm.h"
#include "vm_change_calculator.h"

// ---------------------------------------------------------------------------
// Adaptadores de carrousel: las pantallas se construyen en el FSM (que posee
// los datos); VmCarousel sólo pide funciones libres, así que usamos un
// singleton file-scope (patrón del firmware ESP32 con su base de datos).
// ---------------------------------------------------------------------------
static VmFsm* s_activeFsm = NULL;

static void reposoScreenTrampoline(uint8_t index,
                                   char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_activeFsm != NULL) {
        s_activeFsm->buildReposoScreen(index, lines);
    }
}

static void finScreenTrampoline(uint8_t index,
                                char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_activeFsm != NULL) {
        s_activeFsm->buildFinScreen(index, lines);
    }
}

static void formatMoney(uint32_t centavos, char* buf, size_t size) {
    uint32_t pesos = centavos / 100u;
    uint32_t cents = centavos % 100u;
    snprintf(buf, size, "$%lu.%02lu", (unsigned long)pesos, (unsigned long)cents);
}

// ---------------------------------------------------------------------------
// VmFsm
// ---------------------------------------------------------------------------
VmFsm::VmFsm(VmEepromData& data, VmMotorController& motor, VmRfid& rfid,
              DisplayFn displayFn)
    : _data(data),
      _motor(motor),
      _rfid(rfid),
      _displayFn(displayFn),
      _carousel(displayFn),
      _changeCalc(),
      _state(FsmState::S0_ARRANQUE),
      _mode(VM_MODE_VENTA),
      _dataReady(false),
      _selectedSlot(0),
      _insertedCentavos(0u),
      _stockReserved(false),
      _pendingResult(VM_RESULT_DELIVERED),
      _paymentMethod(0u),
      _pinLen(0),
      _pinFailCount(0u),
      _pinLockoutEnd(0u),
      _lastAdminActivityMs(0u),
      _adminSlot(0),
      _numLen(0),
      _inactivityTimer(0u),
      _motorTimer(0u),
      _doorSampled(false),
      _doorStable(false),
      _doorLastChangeMs(0u),
      _lastKeyDigit(0) {
    memset(&_slotInfo, 0, sizeof(_slotInfo));
    memset(&_changeResult, 0, sizeof(_changeResult));
    memset(_pinBuffer, 0, sizeof(_pinBuffer));
    memset(_numBuffer, 0, sizeof(_numBuffer));

    s_activeFsm = this;
}

void VmFsm::begin() {
    _dataReady = _data.begin();

    if (!_dataReady) {
        enterState(FsmState::S1_FALLA_INTERNA);
        return;
    }

    pinMode(VM_PIN_DOOR, INPUT_PULLUP);
    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);

    _doorSampled = (digitalRead(VM_PIN_DOOR) == VM_DOOR_CLOSED_LEVEL);
    _doorStable = _doorSampled;
    _doorLastChangeMs = millis();

    enterState(FsmState::S0_ARRANQUE);
}

void VmFsm::display(const char* l1, const char* l2,
                    const char* l3, const char* l4) {
    if (_displayFn != NULL) {
        _displayFn(l1, l2, l3, l4);
    }
}

void VmFsm::setMode(uint8_t mode) {
    _mode = mode;
}

void VmFsm::enterState(FsmState next) {
    _state = next;

    switch (next) {
        case FsmState::S0_ARRANQUE:       onEnterArranque(); break;
        case FsmState::S1_FALLA_INTERNA:  onEnterFallaInterna(); break;
        case FsmState::S2_REPOSO:         onEnterReposo(); break;
        case FsmState::S3_SEL_CANAL:      onEnterSelCanal(_selectedSlot); break;
        case FsmState::S4_SEL_PAGO:       onEnterSelPago(); break;
        case FsmState::S5_ESP_EFECTIVO:   onEnterEspEfectivo(); break;
        case FsmState::S6_ESP_RFID:       onEnterEspRfid(); break;
        case FsmState::S7_RESERVADA:      onEnterReservada(); break;
        case FsmState::S8_DISPENSANDO:    onEnterDispensando(); break;
        case FsmState::S9_CONFIRMADA:     onEnterConfirmada(); break;
        case FsmState::S10_FALLA_DISP:    onEnterFallaDisp(); break;
        case FsmState::S11_CALC_CAMBIO:   onEnterCalcCambio(); break;
        case FsmState::S12_PANTALLA_FIN:  onEnterPantallaFin(); break;
        case FsmState::S13_ADMIN_AUTH:    onEnterAdminAuth(); break;
        case FsmState::S14_ADMIN_CANAL:   onEnterAdminCanal(); break;
        case FsmState::S15_ADMIN_ACCION:  onEnterAdminAccion(); break;
        case FsmState::S16_MOD_PRECIO:    onEnterModPrecio(); break;
        case FsmState::S17_MOD_STOCK:     onEnterModStock(); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Actualización por tick
// ---------------------------------------------------------------------------
void VmFsm::update() {
    _motor.poll();
    _carousel.update();
    updateDoor();

    switch (_state) {
        case FsmState::S0_ARRANQUE: {
            if (_dataReady) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S7_RESERVADA: {
            // Error "sin stock": vuelve a reposo tras 1.5 s.
            if (!_stockReserved && (uint32_t)(millis() - _motorTimer) >= 1500u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S8_DISPENSANDO: {
            if (_motor.isBusy()) {
                if (motorTimerExpired()) {
                    _motor.stop();
                    _pendingResult = VM_RESULT_UNCERTAIN;
                    enterState(FsmState::S10_FALLA_DISP);
                }
            } else {
                int result = _motor.consumeResult();
                if (result == VM_RESULT_DELIVERED) {
                    enterState(FsmState::S9_CONFIRMADA);
                } else {
                    _pendingResult = (result > 0) ? (uint8_t)result
                                                  : VM_RESULT_UNCERTAIN;
                    enterState(FsmState::S10_FALLA_DISP);
                }
            }
            break;
        }
        case FsmState::S9_CONFIRMADA: {
            if ((uint32_t)(millis() - _motorTimer) >= 1000u) {
                if (_paymentMethod == 1u) {
                    // RFID: pago exacto, sin cambio.
                    memset(&_changeResult, 0, sizeof(_changeResult));
                    enterState(FsmState::S12_PANTALLA_FIN);
                } else {
                    enterState(FsmState::S11_CALC_CAMBIO);
                }
            }
            break;
        }
        case FsmState::S10_FALLA_DISP: {
            if ((uint32_t)(millis() - _motorTimer) >= 3000u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S11_CALC_CAMBIO: {
            // Si no hubo cambio suficiente, avisa 5 s y regresa a reposo.
            if ((uint32_t)(millis() - _motorTimer) >= 5000u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S12_PANTALLA_FIN: {
            if ((uint32_t)(millis() - _inactivityTimer) >=
                (VM_CAROUSEL_INTERVAL_MS * 2u + 1000u)) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S3_SEL_CANAL:
        case FsmState::S4_SEL_PAGO:
        case FsmState::S5_ESP_EFECTIVO: {
            if (inactivityExpired()) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S6_ESP_RFID: {
            if (inactivityExpired()) {
                enterState(FsmState::S2_REPOSO);
                break;
            }
            // Sondeo no bloqueante del lector RFID.
            RfidReadResult rr = _rfid.poll();
            if (rr == RfidReadResult::OK) {
                uint32_t saldo = _rfid.lastBalance();
                if (saldo >= _slotInfo.priceCentavos) {
                    // Deducir saldo de la tarjeta.
                    RfidWriteResult wr = _rfid.deductBalance(_slotInfo.priceCentavos);
                    if (wr == RfidWriteResult::OK) {
                        _paymentMethod = 1u; // RFID
                        _insertedCentavos = _slotInfo.priceCentavos; // pagó exacto
                        enterState(FsmState::S7_RESERVADA);
                    } else {
                        display("  Error escritura   ",
                                "  en tarjeta RFID   ",
                                "  Reintente.        ",
                                "                    ");
                        _motorTimer = millis();
                        // Vuelve a reposo en 3 s (manejado por S10_FALLA_DISP).
                        enterState(FsmState::S2_REPOSO);
                    }
                } else {
                    // Saldo insuficiente.
                    char l2[VM_DISPLAY_LINE_LEN];
                    char l3[VM_DISPLAY_LINE_LEN];
                    char priceBuf[16], saldoBuf[16];
                    formatMoney(saldo, saldoBuf, sizeof(saldoBuf));
                    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));
                    snprintf(l2, sizeof(l2), " Saldo: %-12s", saldoBuf);
                    snprintf(l3, sizeof(l3), " Precio: %-11s", priceBuf);
                    display("  Saldo insuficiente",
                            l2, l3,
                            "                    ");
                    _motorTimer = millis();
                    // Regresa a reposo tras 3 s (tick de S10).
                    // Usamos un timer inline: esperamos en S6 un momento.
                    delay(3000);
                    enterState(FsmState::S2_REPOSO);
                }
            } else if (rr == RfidReadResult::AUTH_FAILED ||
                       rr == RfidReadResult::READ_FAILED) {
                display("  Error al leer     ",
                        "  la tarjeta RFID   ",
                        "  Reintente.        ",
                        "                    ");
                delay(2000);
                enterState(FsmState::S2_REPOSO);
            }
            // NONE: no hay tarjeta, seguir esperando.
            break;
        }
        case FsmState::S13_ADMIN_AUTH: {
            if ((uint32_t)millis() < _pinLockoutEnd) {
                // Pantalla de lockout: regresa a reposo tras 3 s.
                if ((uint32_t)(millis() - _motorTimer) >= 3000u) {
                    setMode(VM_MODE_VENTA);
                    enterState(FsmState::S2_REPOSO);
                }
            } else if ((uint32_t)(millis() - _lastAdminActivityMs) >=
                       VM_TIMEOUT_INACTIVITY_MS) {
                setMode(VM_MODE_VENTA);
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S14_ADMIN_CANAL:
        case FsmState::S15_ADMIN_ACCION:
        case FsmState::S16_MOD_PRECIO:
        case FsmState::S17_MOD_STOCK: {
            if ((uint32_t)(millis() - _lastAdminActivityMs) >=
                VM_TIMEOUT_INACTIVITY_MS) {
                setMode(VM_MODE_VENTA);
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Entrada a estados: compra y venta
// ---------------------------------------------------------------------------
void VmFsm::onEnterArranque() {
    display("   Iniciando...    ",
            "   Sistema de      ",
            "  Máquina comienza ",
            "   en segundos...  ");
}

void VmFsm::onEnterFallaInterna() {
    display("  ERROR INTERNO    ",
            " EEPROM no disp.   ",
            " Reinicie la bien  ",
            "  o llame al admin ");
}

void VmFsm::onEnterReposo() {
    setMode(VM_MODE_VENTA);
    _carousel.clearScreens();
    _carousel.addScreen(reposoScreenTrampoline);
    _carousel.begin();
    resetInactivityTimer();
}

void VmFsm::onEnterSelCanal(uint8_t slot) {
    _selectedSlot = slot;

    if (!_data.getSlot(slot, _slotInfo) || !_slotInfo.enabled) {
        enterState(FsmState::S2_REPOSO);
        return;
    }

    char priceBuf[16];
    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));

    char l1[VM_DISPLAY_LINE_LEN];
    snprintf(l1, sizeof(l1), "  Seleccionaste: %d ", (int)slot);

    display(l1,
            _slotInfo.productName,
            priceBuf,
            "  [A] Confirmar");
}

void VmFsm::onEnterSelPago() {
    display("  Metodo de pago   ",
            "  [A] Efectivo     ",
            "  [B] Tarjeta RFID ",
            "  [*] Cancelar");
}

void VmFsm::onEnterEspEfectivo() {
    _paymentMethod = 0u;
    _insertedCentavos = 0u;
    resetInactivityTimer();
    renderEfectivoScreen();
}

void VmFsm::renderEfectivoScreen() {
    char money[16];
    char rest[16];
    uint32_t restCentavos = (_slotInfo.priceCentavos > _insertedCentavos)
                                ? (_slotInfo.priceCentavos - _insertedCentavos)
                                : 0u;

    formatMoney(_insertedCentavos, money, sizeof(money));
    formatMoney(restCentavos, rest, sizeof(rest));

    char l3[VM_DISPLAY_LINE_LEN];
    snprintf(l3, sizeof(l3), " Faltan: %-12s", rest);

    display("  Inserta monedas   ",
            money,
            l3,
            "  [B] Cancelar");
}

void VmFsm::onEnterEspRfid() {
    _paymentMethod = 1u;
    _insertedCentavos = 0u;
    resetInactivityTimer();

    char priceBuf[16];
    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));

    char l3[VM_DISPLAY_LINE_LEN];
    snprintf(l3, sizeof(l3), " Precio: %-11s", priceBuf);

    display("  Acerque su        ",
            "  tarjeta al lector ",
            l3,
            "  [B] Cancelar");
}

void VmFsm::onEnterReservada() {
    resetInactivityTimer();
    _stockReserved = false;

    if (!_data.reserveStock(_selectedSlot)) {
        display("  Sin stock         ",
                " Seleccione otro    ",
                "  canal             ",
                "                    ");
        _motorTimer = millis();   // regresa a reposo en 1.5 s
        return;
    }

    _stockReserved = true;
    _data.beginOrder(_selectedSlot);

    // Pre-condiciones físicas: puerta cerrada y motor/barrera libre.
    if (!_doorStable || !_motor.start(_selectedSlot)) {
        _pendingResult = VM_RESULT_REJECTED_BEFORE_MOTION;
        enterState(FsmState::S10_FALLA_DISP);
        return;
    }

    enterState(FsmState::S8_DISPENSANDO);
}

void VmFsm::onEnterDispensando() {
    _motorTimer = millis();

    display("  Despachando...    ",
            _slotInfo.productName,
            "  Espere por favor  ",
            "                    ");
}

void VmFsm::onEnterConfirmada() {
    _stockReserved = false;
    _data.completeOrder(_selectedSlot, VM_RESULT_DELIVERED);
    _motorTimer = millis();

    display("  Compra Exitosa!   ",
            _slotInfo.productName,
            "  Disfruta tu       ",
            "  producto!         ");

    // Si fue RFID, no hay cambio que calcular: saltar a pantalla fin.
    // El tick de S9 espera 1 s antes de avanzar.
}

void VmFsm::onEnterFallaDisp() {
    if (_stockReserved) {
        _stockReserved = false;
        _data.completeOrder(_selectedSlot, _pendingResult);
        _data.releaseStock(_selectedSlot);
    }

    _motorTimer = millis();

    switch (_pendingResult) {
        case VM_RESULT_REJECTED_BEFORE_MOTION:
            display("  No despachado     ",
                    _slotInfo.productName,
                    "  Reintente.        ",
                    "                    ");
            break;
        case VM_RESULT_UNCERTAIN:
            display("  Verifica tu       ",
                    "  entrega. Lineal   ",
                    "  gira de nuevo.    ",
                    "                    ");
            break;
        default:
            display("  Error de despacho ",
                    _slotInfo.productName,
                    "  Vuelve a intentarlo",
                    "                    ");
            break;
    }
}

void VmFsm::onEnterCalcCambio() {
    _motorTimer = millis();

    uint32_t changeTotal = (_insertedCentavos > _slotInfo.priceCentavos)
                               ? (_insertedCentavos - _slotInfo.priceCentavos)
                               : 0u;

    if (changeTotal == 0u) {
        enterState(FsmState::S12_PANTALLA_FIN);
        return;
    }

    memset(&_changeResult, 0, sizeof(_changeResult));

    if (!_changeCalc.calculate(_insertedCentavos, _slotInfo.priceCentavos,
                               _data, _changeResult)) {
        // No se pudo formar el cambio: avisa y vuelve a reposo (tick de S11).
        display("  No hay cambio     ",
                "  disponible.       ",
                "  Avisa al admin.   ",
                "                    ");
        return;
    }

    enterState(FsmState::S12_PANTALLA_FIN);
}

void VmFsm::onEnterPantallaFin() {
    _carousel.clearScreens();
    _carousel.addScreen(finScreenTrampoline);
    _carousel.begin();
    _inactivityTimer = millis();
}

// ---------------------------------------------------------------------------
// Entrada a estados: administración
// ---------------------------------------------------------------------------
void VmFsm::onEnterAdminAuth() {
    if ((uint32_t)millis() < _pinLockoutEnd) {
        display("  Intentos agot.   ",
                "  Espera 30 seg    ",
                "  para reintentar  ",
                "                    ");
        _motorTimer = millis();
        return;
    }

    clearPinBuffer();
    _lastAdminActivityMs = millis();
    renderPinScreen();
}

void VmFsm::renderPinScreen() {
    char l2[VM_DISPLAY_LINE_LEN];
    memset(l2, 0, sizeof(l2));

    char pin[5];
    for (uint8_t i = 0; i < 4u; i++) {
        pin[i] = (i < _pinLen) ? _pinBuffer[i] : ' ';
    }
    snprintf(l2, sizeof(l2), "  %c %c %c %c         ",
             pin[0], pin[1], pin[2], pin[3]);

    display("  Ingresa el PIN   ",
            l2,
            "  [A] Confirmar     ",
            "  [B] Salir         ");
}

void VmFsm::onEnterAdminCanal() {
    _lastAdminActivityMs = millis();

    display("  Elige canal       ",
            "  (1)  Coca-Cola    ",
            "  (2)  Galletas     ",
            "  (5)  Salir        ");
}

void VmFsm::onEnterAdminAccion() {
    _lastAdminActivityMs = millis();

    char l1[VM_DISPLAY_LINE_LEN];
    snprintf(l1, sizeof(l1), "  Canal: %d          ", (int)_adminSlot);

    display(l1,
            "  (1) Precio         ",
            "  (2) Stock          ",
            "  (B) Volver        ");
}

void VmFsm::onEnterModPrecio() {
    clearNumBuffer();
    _lastAdminActivityMs = millis();
    renderNumScreen("  Nuevo precio      ");
}

void VmFsm::onEnterModStock() {
    clearNumBuffer();
    _lastAdminActivityMs = millis();
    renderNumScreen("  Nuevo stock       ");
}

void VmFsm::renderNumScreen(const char* header) {
    char l3[VM_DISPLAY_LINE_LEN];
    memset(l3, 0, sizeof(l3));

    if (_numLen == 0u) {
        snprintf(l3, sizeof(l3), "   $                ");
    } else {
        snprintf(l3, sizeof(l3), "   $%-16s", _numBuffer);
    }

    display(header,
            "  (pesos / piezas) ",
            l3,
            "  [A] Guardar [C] Borrar");
}

// ---------------------------------------------------------------------------
// Handlers de teclado
// ---------------------------------------------------------------------------
void VmFsm::handleKey(char key) {
    KeyMode mode = KeyMode::PAGO;

    switch (_state) {
        case FsmState::S2_REPOSO:         mode = KeyMode::REPOSO; break;
        case FsmState::S5_ESP_EFECTIVO:   mode = KeyMode::EFECTIVO; break;
        case FsmState::S13_ADMIN_AUTH:    mode = KeyMode::ADMIN_PIN; break;
        case FsmState::S14_ADMIN_CANAL:   mode = KeyMode::ADMIN_MENU; break;
        case FsmState::S15_ADMIN_ACCION:  mode = KeyMode::ADMIN_ACCION; break;
        case FsmState::S16_MOD_PRECIO:
        case FsmState::S17_MOD_STOCK:     mode = KeyMode::NUMERICO; break;
        default: break;   // S3/S4: modo PAGO (A confirma, * cancela)
    }

    KeyAction action = _keypad.interpret(key, mode);
    _lastKeyDigit = _keypad.lastDigit();

    switch (_state) {
        case FsmState::S2_REPOSO:         processKeyReposo(action); break;
        case FsmState::S3_SEL_CANAL:      processKeySelCanal(action); break;
        case FsmState::S4_SEL_PAGO:       processKeySelPago(action); break;
        case FsmState::S5_ESP_EFECTIVO:   processKeyEspEfectivo(action); break;
        case FsmState::S6_ESP_RFID: {
            // Solo B cancela la espera de RFID.
            if (action == KeyAction::CHOOSE_RFID ||
                action == KeyAction::CANCEL_ABORT) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        case FsmState::S13_ADMIN_AUTH:    processKeyAdminAuth(action); break;
        case FsmState::S14_ADMIN_CANAL:   processKeyAdminCanal(action); break;
        case FsmState::S15_ADMIN_ACCION:  processKeyAdminAccion(action); break;
        case FsmState::S16_MOD_PRECIO:    processKeyModPrecio(action); break;
        case FsmState::S17_MOD_STOCK:     processKeyModStock(action); break;
        default: break;   // S7..S12: sin teclas relevantes
    }
}

void VmFsm::processKeyReposo(KeyAction a) {
    switch (a) {
        case KeyAction::SELECT_1:
        case KeyAction::SELECT_2:
        case KeyAction::SELECT_3:
        case KeyAction::SELECT_4: {
            _selectedSlot = (uint8_t)a - (uint8_t)KeyAction::SELECT_1 + 1u;
            enterState(FsmState::S3_SEL_CANAL);
            break;
        }
        case KeyAction::ENTER_ADMIN: {
            enterState(FsmState::S13_ADMIN_AUTH);
            break;
        }
        default:
            break;
    }
}

void VmFsm::processKeySelCanal(KeyAction a) {
    switch (a) {
        case KeyAction::CHOOSE_CASH:   // 'A' confirma la selección
            enterState(FsmState::S4_SEL_PAGO);
            break;
        case KeyAction::CANCEL_ABORT:  // '*' cancela
            enterState(FsmState::S2_REPOSO);
            break;
        default:
            break;
    }
}

void VmFsm::processKeySelPago(KeyAction a) {
    switch (a) {
        case KeyAction::CHOOSE_CASH:   // 'A': pagar con efectivo
            enterState(FsmState::S5_ESP_EFECTIVO);
            break;
        case KeyAction::CHOOSE_RFID:   // 'B': pagar con tarjeta RFID
            if (_rfid.isAvailable()) {
                enterState(FsmState::S6_ESP_RFID);
            } else {
                display("  RFID no           ",
                        "  disponible.       ",
                        "  Use efectivo.     ",
                        "                    ");
                // Regresa a sel. pago tras 2 s.
                delay(2000);
                onEnterSelPago();
            }
            break;
        case KeyAction::CANCEL_ABORT:  // '*': cancelar
            enterState(FsmState::S2_REPOSO);
            break;
        default:
            break;
    }
}

void VmFsm::processKeyEspEfectivo(KeyAction a) {
    if (a == KeyAction::CANCEL_ABORT) {   // 'B': abandona el pago
        enterState(FsmState::S2_REPOSO);
        return;
    }

    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_9) {
        uint32_t denom = _keypad.coinActionToCentavos(a);
        if (denom == 0u) {
            return;
        }

        if (!_data.addCoins(denom, 1u)) {
            return;
        }

        _insertedCentavos += denom;
        resetInactivityTimer();

        if (_insertedCentavos >= _slotInfo.priceCentavos) {
            enterState(FsmState::S7_RESERVADA);
            return;
        }

        renderEfectivoScreen();
    }
}

void VmFsm::processKeyAdminAuth(KeyAction a) {
    _lastAdminActivityMs = millis();

    switch (a) {
        case KeyAction::SELECT_NUM: {
            if (_pinLen < 4u) {
                appendPinDigit(_lastKeyDigit);
                renderPinScreen();
            }
            break;
        }
        case KeyAction::CLEAR_BACKSPACE: {
            backspacePinBuffer();
            renderPinScreen();
            break;
        }
        case KeyAction::CONFIRM: {
            if (_pinLen != 4u) {
                display("  PIN incompleto    ",
                        "  Escriba 4 digitos ",
                        "                    ",
                        "                    ");
                _motorTimer = millis();
                return;
            }

            if (_data.verifyAdminPin(_pinBuffer)) {
                _pinFailCount = 0u;
                setMode(VM_MODE_MANTENIMIENTO);
                enterState(FsmState::S14_ADMIN_CANAL);
            } else {
                _pinFailCount++;
                clearPinBuffer();

                if (_pinFailCount >= 3u) {
                    _pinFailCount = 0u;
                    _pinLockoutEnd = millis() + VM_PIN_LOCKOUT_MS;
                    display("  Intentos agot.   ",
                            "  Espera 30 seg    ",
                            "  para reintentar  ",
                            "                    ");
                    _motorTimer = millis();
                } else {
                    renderPinScreen();
                }
            }
            break;
        }
        case KeyAction::CANCEL_ABORT: {
            setMode(VM_MODE_VENTA);
            enterState(FsmState::S2_REPOSO);
            break;
        }
        default:
            break;
    }
}

void VmFsm::processKeyAdminCanal(KeyAction a) {
    _lastAdminActivityMs = millis();

    switch (a) {
        case KeyAction::SELECT_1:
        case KeyAction::SELECT_2:
        case KeyAction::SELECT_3:
        case KeyAction::SELECT_4: {
            _adminSlot = (uint8_t)a - (uint8_t)KeyAction::SELECT_1 + 1u;
            enterState(FsmState::S15_ADMIN_ACCION);
            break;
        }
        case KeyAction::CANCEL_ABORT: {   // '5' = salir
            setMode(VM_MODE_VENTA);
            enterState(FsmState::S2_REPOSO);
            break;
        }
        default:
            break;
    }
}

void VmFsm::processKeyAdminAccion(KeyAction a) {
    _lastAdminActivityMs = millis();

    switch (a) {
        case KeyAction::SELECT_1:
            enterState(FsmState::S16_MOD_PRECIO);
            break;
        case KeyAction::SELECT_2:
            enterState(FsmState::S17_MOD_STOCK);
            break;
        case KeyAction::CANCEL_ABORT:   // 'B' = volver
            enterState(FsmState::S14_ADMIN_CANAL);
            break;
        default:
            break;
    }
}

void VmFsm::processKeyModPrecio(KeyAction a) {
    _lastAdminActivityMs = millis();

    switch (a) {
        case KeyAction::SELECT_NUM: {
            if (_numLen < 6u) {
                appendNumBuffer(_lastKeyDigit);
                renderNumScreen("  Nuevo precio      ");
            }
            break;
        }
        case KeyAction::CLEAR_BACKSPACE: {
            backspaceNumBuffer();
            renderNumScreen("  Nuevo precio      ");
            break;
        }
        case KeyAction::CONFIRM: {
            uint32_t pesos = numBufferValue();
            if (pesos > 0u && _data.updateSlotPrice(_adminSlot, pesos * 100u)) {
                enterState(FsmState::S15_ADMIN_ACCION);
            } else {
                enterState(FsmState::S15_ADMIN_ACCION);
            }
            break;
        }
        case KeyAction::CANCEL_ABORT: {
            enterState(FsmState::S15_ADMIN_ACCION);
            break;
        }
        default:
            break;
    }
}

void VmFsm::processKeyModStock(KeyAction a) {
    _lastAdminActivityMs = millis();

    switch (a) {
        case KeyAction::SELECT_NUM: {
            if (_numLen < 6u) {
                appendNumBuffer(_lastKeyDigit);
                renderNumScreen("  Nuevo stock       ");
            }
            break;
        }
        case KeyAction::CLEAR_BACKSPACE: {
            backspaceNumBuffer();
            renderNumScreen("  Nuevo stock       ");
            break;
        }
        case KeyAction::CONFIRM: {
            uint32_t stock = numBufferValue();
            if (_data.updateSlotStock(_adminSlot, stock)) {
                enterState(FsmState::S15_ADMIN_ACCION);
            }
            break;
        }
        case KeyAction::CANCEL_ABORT: {
            enterState(FsmState::S15_ADMIN_ACCION);
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Carrousel
// ---------------------------------------------------------------------------
void VmFsm::buildReposoScreen(uint8_t index,
                              char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);

    snprintf(lines[0], LCD_LINE_LEN, "  Selecciona un canal ");

    uint8_t start = (index * 2u) % 4u;
    for (uint8_t i = 0; i < 2u; i++) {
        uint8_t slotNum = start + i + 1u;
        SlotInfo info;
        memset(&info, 0, sizeof(info));
        if (_data.getSlot(slotNum, info)) {
            char priceBuf[16];
            formatMoney(info.priceCentavos, priceBuf, sizeof(priceBuf));
            snprintf(lines[1 + 2 * i], LCD_LINE_LEN, "%d) %-16s",
                     (int)slotNum, info.productName);
            snprintf(lines[2 + 2 * i], LCD_LINE_LEN, "    %s  Stock:%lu",
                     priceBuf, (unsigned long)info.stock);
        }
    }
}

void VmFsm::buildFinScreen(uint8_t index,
                           char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);

    if (index == 0u) {
        snprintf(lines[0], LCD_LINE_LEN, "  Compra Exitosa     ");
        snprintf(lines[1], LCD_LINE_LEN, "  %-18s", _slotInfo.productName);
        snprintf(lines[2], LCD_LINE_LEN, "  Disfruta tu        ");
        snprintf(lines[3], LCD_LINE_LEN, "  producto!          ");
        return;
    }

    if (_paymentMethod == 1u) {
        // Pago con RFID: mostrar resumen sin cambio.
        char priceBuf[16];
        formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));

        char saldoBuf[16];
        formatMoney(_rfid.lastBalance(), saldoBuf, sizeof(saldoBuf));

        snprintf(lines[0], LCD_LINE_LEN, " Cobrado con RFID   ");
        snprintf(lines[1], LCD_LINE_LEN, " Monto: %-12s", priceBuf);
        snprintf(lines[2], LCD_LINE_LEN, " Saldo rest: %-7s", saldoBuf);
        snprintf(lines[3], LCD_LINE_LEN, "  Gracias!           ");
        return;
    }

    // Pago en efectivo: desglose de cambio.
    uint32_t changeTotal = (_insertedCentavos > _slotInfo.priceCentavos)
                               ? (_insertedCentavos - _slotInfo.priceCentavos)
                               : 0u;

    char money[16];
    formatMoney(changeTotal, money, sizeof(money));

    char l1[VM_DISPLAY_LINE_LEN];
    snprintf(l1, sizeof(l1), " Tu cambio: %-10s", money);

    snprintf(lines[0], LCD_LINE_LEN, "%s", l1);
    snprintf(lines[1], LCD_LINE_LEN,
             "  $1000:%lu $500:%lu", (unsigned long)_changeResult.coin1000,
             (unsigned long)_changeResult.coin500);
    snprintf(lines[2], LCD_LINE_LEN,
             "  $200:%lu  $100:%lu ", (unsigned long)_changeResult.coin200,
             (unsigned long)_changeResult.coin100);
    snprintf(lines[3], LCD_LINE_LEN, "  Recoge tu cambio   ");
}

// ---------------------------------------------------------------------------
// Utilidades de edición de buffers
// ---------------------------------------------------------------------------
uint32_t VmFsm::numBufferValue() const {
    uint32_t v = 0u;
    for (uint8_t i = 0; i < _numLen; i++) {
        v = v * 10u + (uint32_t)(_numBuffer[i] - '0');
    }
    return v;
}

void VmFsm::appendNumBuffer(uint8_t digitVal) {
    if (_numLen >= 6u) {
        return;
    }
    _numBuffer[_numLen++] = (char)('0' + digitVal);
    _numBuffer[_numLen] = '\0';
}

void VmFsm::backspaceNumBuffer() {
    if (_numLen > 0u) {
        _numLen--;
    }
    _numBuffer[_numLen] = '\0';
}

void VmFsm::appendPinDigit(uint8_t digitVal) {
    if (_pinLen >= 4u) {
        return;
    }
    _pinBuffer[_pinLen++] = (char)('0' + digitVal);
    _pinBuffer[_pinLen] = '\0';
}

void VmFsm::backspacePinBuffer() {
    if (_pinLen > 0u) {
        _pinLen--;
    }
    _pinBuffer[_pinLen] = '\0';
}

// ---------------------------------------------------------------------------
// Sensor de puerta (debounce)
// ---------------------------------------------------------------------------
void VmFsm::updateDoor() {
    bool level = (digitalRead(VM_PIN_DOOR) == VM_DOOR_CLOSED_LEVEL);

    if (level != _doorSampled) {
        _doorSampled = level;
        _doorLastChangeMs = millis();
    } else if ((uint32_t)(millis() - _doorLastChangeMs) >= VM_DOOR_DEBOUNCE_MS) {
        _doorStable = level;
    }
}