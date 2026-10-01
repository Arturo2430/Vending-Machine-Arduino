#include <string.h>
#include "vm_fsm.h"
#include "vm_change_calculator.h"

// Puntero estatico para delegar callbacks del carrusel a la instancia FSM
static VmFsm* s_fsm = NULL;

static void reposoScreenTrampoline(uint8_t idx,
                                   char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildReposoScreen(idx, lines);
}

static void finSuccessTrampoline(uint8_t idx,
                                 char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildFinSuccessScreen(idx, lines);
}

static void finRfidTrampoline(uint8_t idx,
                               char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildFinRfidScreen(idx, lines);
}

static void changeScreenTrampoline(uint8_t idx,
                                    char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildChangeScreen(idx, lines);
}

static void formatMoney(uint32_t centavos, char* buf, size_t size) {
    uint32_t pesos = centavos / 100u;
    uint32_t cents = centavos % 100u;
    snprintf(buf, size, "$%lu.%02lu", (unsigned long)pesos, (unsigned long)cents);
}

VmFsm::VmFsm(VmEepromData& data, VmMotorController& motor, VmRfid& rfid,
              DisplayFn displayFn)
    : _data(data),
      _motor(motor),
      _rfid(rfid),
      _displayFn(displayFn),
      _carousel(displayFn),
      _changeCalc(),
      _state(FsmState::S0_ARRANQUE),
      _selectedSlot(0),
      _insertedCentavos(0u),
      _stockReserved(false),
      _pendingResult(VM_RESULT_DELIVERED),
      _paymentMethod(0u),
      _changeSlides(0),
      _inactivityTimer(0u),
      _motorTimer(0u)
{
    memset(&_slotInfo,     0, sizeof(_slotInfo));
    memset(&_changeResult, 0, sizeof(_changeResult));
    memset(_changeDenoms,  0, sizeof(_changeDenoms));
    memset(_changeQtys,    0, sizeof(_changeQtys));
    s_fsm = this;
}

void VmFsm::begin() {
    if (!_data.begin()) {
        enterState(FsmState::S1_FALLA_INTERNA);
        return;
    }

    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);
    enterState(FsmState::S0_ARRANQUE);
}

void VmFsm::display(const char* l1, const char* l2,
                    const char* l3, const char* l4) {
    if (_displayFn) _displayFn(l1, l2, l3, l4);
}

void VmFsm::update() {
    _motor.poll();
    _carousel.update();

    switch (_state) {
        case FsmState::S0_ARRANQUE:
            enterState(FsmState::S2_REPOSO);
            break;

        case FsmState::S3_SEL_CANAL:
        case FsmState::S4_SEL_PAGO:
        case FsmState::S5_ESP_EFECTIVO:
            if (inactivityExpired()) {
                enterState(FsmState::S2_REPOSO);
            }
            break;

        case FsmState::S6_ESP_RFID: {
            if (inactivityExpired()) {
                enterState(FsmState::S2_REPOSO);
                break;
            }

            RfidReadResult rr = _rfid.poll();
            if (rr == RfidReadResult::OK) {
                uint32_t saldo = _rfid.lastBalance();

                if (saldo >= _slotInfo.priceCentavos) {
                    RfidWriteResult wr = _rfid.deductBalance(_slotInfo.priceCentavos);
                    if (wr == RfidWriteResult::OK) {
                        _paymentMethod    = 1u;
                        _insertedCentavos = _slotInfo.priceCentavos;
                        enterState(FsmState::S7_RESERVADA);
                    } else {
                        display("  Error escritura   ",
                                "  en tarjeta RFID   ",
                                "  Reintente.        ",
                                "                    ");
                        delay(2000);
                        enterState(FsmState::S2_REPOSO);
                    }
                } else {
                    char l2[VM_DISPLAY_LINE_LEN], l3[VM_DISPLAY_LINE_LEN];
                    char priceBuf[16], saldoBuf[16];
                    formatMoney(saldo,                  saldoBuf, sizeof(saldoBuf));
                    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));
                    snprintf(l2, sizeof(l2), " Saldo: %-12s", saldoBuf);
                    snprintf(l3, sizeof(l3), " Precio: %-11s", priceBuf);
                    display("  Saldo insuficiente", l2, l3, "                    ");
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
            break;
        }

        case FsmState::S7_RESERVADA:
            if (!_stockReserved && (uint32_t)(millis() - _motorTimer) >= 1500u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;

        case FsmState::S8_DISPENSANDO:
            if (_motor.isBusy()) {
                if (motorTimerExpired()) {
                    _motor.stop();
                    _pendingResult = VM_RESULT_UNCERTAIN;
                    enterState(FsmState::S10_FALLA_DISP);
                }
            } else {
                int res = _motor.consumeResult();
                if (res == VM_RESULT_DELIVERED) {
                    enterState(FsmState::S9_CONFIRMADA);
                } else {
                    _pendingResult = (res >= 0) ? (uint8_t)res : VM_RESULT_UNCERTAIN;
                    enterState(FsmState::S10_FALLA_DISP);
                }
            }
            break;

        case FsmState::S9_CONFIRMADA:
            if ((uint32_t)(millis() - _motorTimer) >= 1000u) {
                if (_paymentMethod == 1u) {
                    memset(&_changeResult, 0, sizeof(_changeResult));
                    enterState(FsmState::S12_PANTALLA_FIN);
                } else {
                    enterState(FsmState::S11_CALC_CAMBIO);
                }
            }
            break;

        case FsmState::S10_FALLA_DISP:
            if ((uint32_t)(millis() - _motorTimer) >= 3000u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;

        case FsmState::S11_CALC_CAMBIO:
            if ((uint32_t)(millis() - _motorTimer) >= 5000u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;

        case FsmState::S12_PANTALLA_FIN: {
            uint8_t numSlides = 1u + _changeSlides;
            unsigned long totalMs = (unsigned long)numSlides * VM_CAROUSEL_INTERVAL_MS + 1000UL;
            if ((uint32_t)(millis() - _inactivityTimer) >= totalMs) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }

        default:
            break;
    }
}

void VmFsm::enterState(FsmState next) {
    _state = next;

    switch (next) {
        case FsmState::S0_ARRANQUE:      onEnterArranque();              break;
        case FsmState::S1_FALLA_INTERNA: onEnterFallaInterna();          break;
        case FsmState::S2_REPOSO:        onEnterReposo();                break;
        case FsmState::S3_SEL_CANAL:     onEnterSelCanal(_selectedSlot); break;
        case FsmState::S4_SEL_PAGO:      onEnterSelPago();               break;
        case FsmState::S5_ESP_EFECTIVO:  onEnterEspEfectivo();           break;
        case FsmState::S6_ESP_RFID:      onEnterEspRfid();               break;
        case FsmState::S7_RESERVADA:     onEnterReservada();             break;
        case FsmState::S8_DISPENSANDO:   onEnterDispensando();           break;
        case FsmState::S9_CONFIRMADA:    onEnterConfirmada();            break;
        case FsmState::S10_FALLA_DISP:   onEnterFallaDisp();             break;
        case FsmState::S11_CALC_CAMBIO:  onEnterCalcCambio();            break;
        case FsmState::S12_PANTALLA_FIN: onEnterPantallaFin();           break;
        default: break;
    }
}

void VmFsm::onEnterArranque() {
    display("   Iniciando...    ",
            "   Maquina         ",
            "   Expendedora     ",
            "                   ");
}

void VmFsm::onEnterFallaInterna() {
    display("  ERROR INTERNO    ",
            "  EEPROM no disp.  ",
            "  Reinicie o llame ",
            "  al tecnico.      ");
}

void VmFsm::onEnterReposo() {
    _carousel.clearScreens();
    _carousel.addScreen(reposoScreenTrampoline); // slots 1 y 2
    _carousel.addScreen(reposoScreenTrampoline); // slots 3 y 4
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
            "  [A] Confirmar     ");

    resetInactivityTimer();
}

void VmFsm::onEnterSelPago() {
    display("  Metodo de pago   ",
            "  [A] Efectivo     ",
            "  [B] Tarjeta RFID ",
            "  [*] Cancelar     ");
    resetInactivityTimer();
}

void VmFsm::onEnterEspEfectivo() {
    _paymentMethod    = 0u;
    _insertedCentavos = 0u;
    resetInactivityTimer();
    renderEfectivoScreen();
}

void VmFsm::onEnterEspRfid() {
    _paymentMethod    = 1u;
    _insertedCentavos = 0u;
    resetInactivityTimer();

    char priceBuf[16];
    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));

    char l3[VM_DISPLAY_LINE_LEN];
    snprintf(l3, sizeof(l3), " Precio: %-11s", priceBuf);

    display("  Acerque su        ",
            "  tarjeta al lector ",
            l3,
            "  [B] Cancelar     ");
}

void VmFsm::onEnterReservada() {
    resetInactivityTimer();
    _stockReserved = false;

    if (!_data.reserveStock(_selectedSlot)) {
        display("  Sin stock         ",
                "  Seleccione otro   ",
                "  canal.            ",
                "                    ");
        _motorTimer = millis(); // 1.5s antes de volver a reposo
        return;
    }

    _stockReserved = true;

    if (!_motor.start(_selectedSlot)) {
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
    _motorTimer    = millis();

    display("  Compra Exitosa!   ",
            _slotInfo.productName,
            "  Disfruta tu       ",
            "  producto!         ");
}

void VmFsm::onEnterFallaDisp() {
    if (_stockReserved) {
        _stockReserved = false;
        _data.releaseStock(_selectedSlot);
    }

    _motorTimer = millis();

    switch (_pendingResult) {
        case VM_RESULT_REJECTED_BEFORE_MOTION:
            display("  No se pudo        ",
                    "  despachar.        ",
                    "  Reintente.        ",
                    "                    ");
            break;
        case VM_RESULT_UNCERTAIN:
            display("  Revisa tu         ",
                    "  producto.         ",
                    "  El motor giro.    ",
                    "                    ");
            break;
        default:
            display("  Error de despacho ",
                    _slotInfo.productName,
                    "  Intente de nuevo  ",
                    "                    ");
            break;
    }
}

void VmFsm::onEnterCalcCambio() {
    _motorTimer = millis();

    uint32_t cambio = (_insertedCentavos > _slotInfo.priceCentavos)
                          ? (_insertedCentavos - _slotInfo.priceCentavos) : 0u;

    if (cambio == 0u) {
        enterState(FsmState::S12_PANTALLA_FIN);
        return;
    }

    memset(&_changeResult, 0, sizeof(_changeResult));

    bool ok = _changeCalc.calculate(_insertedCentavos,
                                    _slotInfo.priceCentavos,
                                    _data,
                                    _changeResult);
    if (!ok) {
        display("  No hay cambio     ",
                "  disponible.       ",
                "  Avisa al tecnico. ",
                "                    ");
        return;
    }

    enterState(FsmState::S12_PANTALLA_FIN);
}

void VmFsm::onEnterPantallaFin() {
    _carousel.clearScreens();
    _changeSlides = 0;

    _carousel.addScreen(finSuccessTrampoline);

    if (_paymentMethod == 1u) {
        _carousel.addScreen(finRfidTrampoline);
        _changeSlides = 1;
    } else {
        const uint32_t denoms[4] = {1000u, 500u, 200u, 100u};
        const uint32_t qtys[4]   = {
            _changeResult.coin1000,
            _changeResult.coin500,
            _changeResult.coin200,
            _changeResult.coin100
        };

        for (uint8_t i = 0; i < 4; i++) {
            if (qtys[i] > 0u) {
                _changeDenoms[_changeSlides] = denoms[i];
                _changeQtys[_changeSlides]   = qtys[i];
                _changeSlides++;
                _carousel.addScreen(changeScreenTrampoline);
            }
        }
    }

    _carousel.begin();
    _inactivityTimer = millis();
}

void VmFsm::handleKey(char key) {
    KeyMode mode = KeyMode::PAGO;
    if (_state == FsmState::S2_REPOSO)       mode = KeyMode::REPOSO;
    if (_state == FsmState::S5_ESP_EFECTIVO) mode = KeyMode::EFECTIVO;

    KeyAction action = _keypad.interpret(key, mode);

    switch (_state) {
        case FsmState::S2_REPOSO:        processKeyReposo(action);      break;
        case FsmState::S3_SEL_CANAL:     processKeySelCanal(action);    break;
        case FsmState::S4_SEL_PAGO:      processKeySelPago(action);     break;
        case FsmState::S5_ESP_EFECTIVO:  processKeyEspEfectivo(action); break;
        case FsmState::S6_ESP_RFID:
            if (action == KeyAction::CANCEL_ABORT ||
                action == KeyAction::CHOOSE_RFID) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        default:
            break;
    }
}

void VmFsm::processKeyReposo(KeyAction a) {
    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_4) {
        _selectedSlot = (uint8_t)a - (uint8_t)KeyAction::SELECT_1 + 1u;
        enterState(FsmState::S3_SEL_CANAL);
    }
}

void VmFsm::processKeySelCanal(KeyAction a) {
    if (a == KeyAction::CHOOSE_CASH) {
        enterState(FsmState::S4_SEL_PAGO);
    } else if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_REPOSO);
    }
}

void VmFsm::processKeySelPago(KeyAction a) {
    if (a == KeyAction::CHOOSE_CASH) {
        enterState(FsmState::S5_ESP_EFECTIVO);
    } else if (a == KeyAction::CHOOSE_RFID) {
        if (_rfid.isAvailable()) {
            enterState(FsmState::S6_ESP_RFID);
        } else {
            display("  RFID no           ",
                    "  disponible.       ",
                    "  Use efectivo.     ",
                    "                    ");
            delay(2000);
            onEnterSelPago();
        }
    } else if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_REPOSO);
    }
}

void VmFsm::processKeyEspEfectivo(KeyAction a) {
    if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_REPOSO);
        return;
    }

    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_4) {
        uint32_t denom = _keypad.coinActionToCentavos(a);
        if (denom == 0u) return;

        if (!_data.addCoins(denom, 1u)) return;

        _insertedCentavos += denom;
        resetInactivityTimer();

        if (_insertedCentavos >= _slotInfo.priceCentavos) {
            enterState(FsmState::S7_RESERVADA);
            return;
        }

        renderEfectivoScreen();
    }
}

void VmFsm::buildReposoScreen(uint8_t idx,
                               char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    snprintf(lines[0], LCD_LINE_LEN, "  Selecciona canal  ");

    uint8_t start = (idx % 2u) * 2u; // 0 -> slots 1 y 2, 1 -> slots 3 y 4
    for (uint8_t i = 0; i < 2u; i++) {
        uint8_t slotNum = start + i + 1u;
        SlotInfo info;
        memset(&info, 0, sizeof(info));
        if (_data.getSlot(slotNum, info)) {
            char priceBuf[16];
            formatMoney(info.priceCentavos, priceBuf, sizeof(priceBuf));
            snprintf(lines[1 + 2 * i], LCD_LINE_LEN,
                     "%d) %-16s", (int)slotNum, info.productName);
            snprintf(lines[2 + 2 * i], LCD_LINE_LEN,
                     "    %s Stock:%lu", priceBuf, (unsigned long)info.stock);
        }
    }
}

void VmFsm::buildFinSuccessScreen(uint8_t /*idx*/,
                                   char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    snprintf(lines[0], LCD_LINE_LEN, "  Compra Exitosa!    ");
    snprintf(lines[1], LCD_LINE_LEN, "  %-18s", _slotInfo.productName);
    snprintf(lines[2], LCD_LINE_LEN, "  Disfruta tu        ");
    snprintf(lines[3], LCD_LINE_LEN, "  producto!          ");
}

void VmFsm::buildFinRfidScreen(uint8_t /*idx*/,
                                char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    char priceBuf[16], saldoBuf[16];
    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));
    formatMoney(_rfid.lastBalance(),     saldoBuf, sizeof(saldoBuf));
    snprintf(lines[0], LCD_LINE_LEN, " Cobrado con RFID   ");
    snprintf(lines[1], LCD_LINE_LEN, " Monto: %-12s", priceBuf);
    snprintf(lines[2], LCD_LINE_LEN, " Saldo rest: %-7s", saldoBuf);
    snprintf(lines[3], LCD_LINE_LEN, "  Gracias!           ");
}

void VmFsm::buildChangeScreen(uint8_t idx,
                               char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_COUNT * LCD_LINE_LEN);

    // idx=1 es la primera slide de cambio (idx=0 es éxito)
    uint8_t denomIdx = (idx > 0u) ? (idx - 1u) : 0u;
    if (denomIdx >= _changeSlides) return;

    uint32_t denom = _changeDenoms[denomIdx];
    uint32_t qty   = _changeQtys[denomIdx];

    char denomBuf[16];
    formatMoney(denom, denomBuf, sizeof(denomBuf));

    char l3[VM_DISPLAY_LINE_LEN];
    snprintf(l3, sizeof(l3), "  %-7s x %lu pieza%s",
             denomBuf, (unsigned long)qty, (qty == 1u ? "" : "s"));

    snprintf(lines[0], LCD_LINE_LEN, "  Tu cambio:        ");
    snprintf(lines[1], LCD_LINE_LEN, "  Denominacion:     ");
    snprintf(lines[2], LCD_LINE_LEN, "%s", l3);
    snprintf(lines[3], LCD_LINE_LEN, "  Recoge tu cambio  ");
}

void VmFsm::renderEfectivoScreen() {
    uint32_t falta = (_slotInfo.priceCentavos > _insertedCentavos)
                         ? (_slotInfo.priceCentavos - _insertedCentavos) : 0u;
    char moneyBuf[16], faltaBuf[16];
    formatMoney(_insertedCentavos, moneyBuf, sizeof(moneyBuf));
    formatMoney(falta,             faltaBuf, sizeof(faltaBuf));

    char l2[VM_DISPLAY_LINE_LEN];
    char l3[VM_DISPLAY_LINE_LEN];
    snprintf(l2, sizeof(l2), " Insertado: %-9s", moneyBuf);
    snprintf(l3, sizeof(l3), " Faltan:    %-9s", faltaBuf);

    display("  Inserta monedas   ",
            l2,
            l3,
            " 1=$1 2=$2 3=$5 4=$10");
}