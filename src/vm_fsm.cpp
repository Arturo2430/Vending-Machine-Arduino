#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "vm_fsm.h"
#include "vm_change_calculator.h"

static_assert(LCD_LINE_LEN >= 21, "LCD_LINE_LEN debe ser >= 21 (20 columnas + nul)");
static_assert(VM_DISPLAY_LINE_LEN >= 21, "VM_DISPLAY_LINE_LEN debe ser >= 21");

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

static void finRefundTrampoline(uint8_t idx,
                                char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildFinRefundScreen(idx, lines);
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

static void setLinef(char* dst, const char* fmt, ...) {
    char tmp[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    snprintf(dst, LCD_LINE_LEN, "%-20.20s", tmp);
}

VmFsm::VmFsm(VmEepromData& data, VmMotorController& motor, VmRfid& rfid,
              DisplayFn displayFn)
    : _data(data),
      _motor(motor),
      _rfid(rfid),
      _displayFn(displayFn),
      _carousel(displayFn),
      _changeCalc(),
      _state(FsmState::S0_START),
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

    _refundMode       = false;
    _refundCentavos   = 0u;
    _owedCentavos     = 0u;
    _rfidBalanceAfter = 0u;

    s_fsm = this;
}

void VmFsm::begin() {
    if (!_data.begin()) {
        enterState(FsmState::S1_INTERNAL_ERROR);
        return;
    }

    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);
    enterState(FsmState::S0_START);
}

void VmFsm::display(const char* l1, const char* l2,
                    const char* l3, const char* l4) {
    if (_displayFn) _displayFn(l1, l2, l3, l4);
}

void VmFsm::startRefund(uint32_t centavos) {
    _refundMode     = true;
    _refundCentavos = centavos;
    enterState(FsmState::S11_CALC_CHANGE);
}

void VmFsm::update() {
    _motor.poll();
    _carousel.update();

    switch (_state) {
        case FsmState::S0_START:
            enterState(FsmState::S2_STANDBY);
            break;

        case FsmState::S3_SELECT_CHANNEL:
        case FsmState::S4_SELECT_PAYMENT:
        case FsmState::S5_WAIT_CASH:
            if (inactivityExpired()) {
                if (_insertedCentavos > 0u) {
                    startRefund(_insertedCentavos);
                } else {
                    enterState(FsmState::S2_STANDBY);
                }
            }
            break;

        case FsmState::S6_WAIT_RFID: {
            if (inactivityExpired()) {
                enterState(FsmState::S2_STANDBY);
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
                        _rfidBalanceAfter = saldo - _slotInfo.priceCentavos;
                        enterState(FsmState::S7_PREPARING_VEND);
                    } else {
                        showMessage("  Error escritura   ",
                                    "  en tarjeta RFID   ",
                                    "  Reintente.        ",
                                    "  [A] Continuar     ",
                                    FsmState::S2_STANDBY);
                    }
                } else {
                    char l2[VM_DISPLAY_LINE_LEN], l3[VM_DISPLAY_LINE_LEN];
                    char priceBuf[16], saldoBuf[16];
                    formatMoney(saldo,                  saldoBuf, sizeof(saldoBuf));
                    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));
                    snprintf(l2, sizeof(l2), " Saldo: %-12s", saldoBuf);
                    snprintf(l3, sizeof(l3), " Precio: %-11s", priceBuf);
                    showMessage("  Saldo insuficiente", l2, l3, "  [A] Continuar     ", FsmState::S2_STANDBY);
                }
            } else if (rr == RfidReadResult::AUTH_FAILED ||
                       rr == RfidReadResult::READ_FAILED) {
                showMessage("  Error al leer     ",
                            "  la tarjeta RFID   ",
                            "  Reintente.        ",
                            "  [A] Continuar     ",
                            FsmState::S2_STANDBY);
            }
            break;
        }

        case FsmState::S7_PREPARING_VEND:
            break;

        case FsmState::S8_DISPENSING:
            if (_motor.isBusy()) {
                if (motorTimerExpired()) {
                    _motor.stop();
                    _pendingResult = VM_RESULT_UNCERTAIN;
                    enterState(FsmState::S10_VEND_ERROR);
                }
            } else {
                int res = _motor.consumeResult();
                if (res == VM_RESULT_DELIVERED) {
                    enterState(FsmState::S9_CONFIRMED);
                } else {
                    _pendingResult = (res >= 0) ? (uint8_t)res : VM_RESULT_UNCERTAIN;
                    enterState(FsmState::S10_VEND_ERROR);
                }
            }
            break;

        case FsmState::S9_CONFIRMED:
            if ((uint32_t)(millis() - _motorTimer) >= 1000u) {
                _refundMode = false;
                if (_paymentMethod == 1u) {
                    memset(&_changeResult, 0, sizeof(_changeResult));
                    enterState(FsmState::S12_FINISH_SCREEN);
                } else {
                    enterState(FsmState::S11_CALC_CHANGE);
                }
            }
            break;

        case FsmState::S10_VEND_ERROR:
            if ((uint32_t)(millis() - _motorTimer) >= 2500u) {
                if (_refundCentavos > 0u) {
                    enterState(FsmState::S11_CALC_CHANGE);   // devolver monedas
                } else if (_paymentMethod == 1u) {
                    enterState(FsmState::S12_FINISH_SCREEN); // ya reembolsado a tarjeta
                } else {
                    enterState(FsmState::S2_STANDBY);
                }
            }
            break;

        // sale hacia S12 (exito) o S13 (adeudo registrado).
        case FsmState::S11_CALC_CHANGE:
            break;

        case FsmState::S13_MESSAGE_PROMPT:
            if (inactivityExpired()) {
                enterState(FsmState::S2_STANDBY);
            }
            break;

        case FsmState::S12_FINISH_SCREEN: {
            uint8_t numSlides = 1u + _changeSlides;
            unsigned long totalMs = (unsigned long)numSlides * VM_CAROUSEL_INTERVAL_MS + 1000UL;
            if ((uint32_t)(millis() - _inactivityTimer) >= totalMs) {
                enterState(FsmState::S2_STANDBY);
            }
            break;
        }

        default:
            break;
    }
}

void VmFsm::enterState(FsmState next) {
    _state = next;
    _carousel.clearScreens();

    switch (next) {
        case FsmState::S0_START:          onEnterStart();                      break;
        case FsmState::S1_INTERNAL_ERROR: onEnterInternalError();              break;
        case FsmState::S2_STANDBY:        onEnterStandby();                    break;
        case FsmState::S3_SELECT_CHANNEL: onEnterSelectChannel(_selectedSlot); break;
        case FsmState::S4_SELECT_PAYMENT: onEnterSelectPayment();              break;
        case FsmState::S5_WAIT_CASH:      onEnterWaitCash();                   break;
        case FsmState::S6_WAIT_RFID:      onEnterWaitRfid();                   break;
        case FsmState::S7_PREPARING_VEND: onEnterPreparingVend();              break;
        case FsmState::S8_DISPENSING:     onEnterDispensing();                 break;
        case FsmState::S9_CONFIRMED:      onEnterConfirmed();                  break;
        case FsmState::S10_VEND_ERROR:    onEnterVendError();                  break;
        case FsmState::S11_CALC_CHANGE:   onEnterCalcChange();                 break;
        case FsmState::S12_FINISH_SCREEN: onEnterFinishScreen();               break;
        case FsmState::S13_MESSAGE_PROMPT: onEnterMessagePrompt();             break;
        default: break;
    }
}

void VmFsm::onEnterStart() {
    display("   Iniciando...    ",
            "   Maquina         ",
            "   Expendedora     ",
            "                   ");
}

void VmFsm::onEnterInternalError() {
    display("  ERROR INTERNO    ",
            "  EEPROM no disp.  ",
            "  Reinicie o llame ",
            "  al tecnico.      ");
}

void VmFsm::onEnterStandby() {
    _insertedCentavos = 0u;
    _paymentMethod    = 0u;
    _refundMode       = false;
    _refundCentavos   = 0u;
    _rfidBalanceAfter = 0u;
    memset(&_changeResult, 0, sizeof(_changeResult));

    _carousel.clearScreens();
    for (uint8_t i = 0; i < 4; i++) {
        _carousel.addScreen(reposoScreenTrampoline);
    }
    // Pantalla extra si hay un adeudo pendiente con algun cliente
    if (_owedCentavos > 0u) {
        _carousel.addScreen(reposoScreenTrampoline);
    }
    _carousel.begin();
    resetInactivityTimer();
}

void VmFsm::onEnterSelectChannel(uint8_t slot) {
    _selectedSlot = slot;

    if (!_data.getSlot(slot, _slotInfo) || !_slotInfo.enabled) {
        enterState(FsmState::S2_STANDBY);
        return;
    }

    if (_slotInfo.stock == 0) {
        showMessage("  Producto agotado  ",
                    "  Seleccione otro   ",
                    "                    ",
                    "  [A] Continuar     ",
                    FsmState::S2_STANDBY);
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

void VmFsm::onEnterSelectPayment() {
    display("  Metodo de pago   ",
            "  [A] Efectivo     ",
            "  [B] Tarjeta RFID ",
            "  [*] Cancelar     ");
    resetInactivityTimer();
}

void VmFsm::onEnterWaitCash() {
    _paymentMethod    = 0u;
    _insertedCentavos = 0u;
    resetInactivityTimer();
    renderCashScreen();
}

void VmFsm::onEnterWaitRfid() {
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

void VmFsm::onEnterPreparingVend() {
    resetInactivityTimer();
    _stockReserved = false;

    if (!_data.reserveStock(_selectedSlot)) {
        _pendingResult = VM_RESULT_REJECTED_BEFORE_MOTION;
        enterState(FsmState::S10_VEND_ERROR);   // reembolsa lo pagado
        return;
    }
    _stockReserved = true;

    if (!_motor.start(_selectedSlot)) {
        _pendingResult = VM_RESULT_REJECTED_BEFORE_MOTION;
        enterState(FsmState::S10_VEND_ERROR);
        return;
    }

    enterState(FsmState::S8_DISPENSING);
}

void VmFsm::onEnterDispensing() {
    _motorTimer = millis();
    display("  Despachando...    ",
            _slotInfo.productName,
            "  Espere por favor  ",
            "                    ");
}

void VmFsm::onEnterConfirmed() {
    _stockReserved = false;
    _motorTimer    = millis();

    display("  Compra Exitosa!   ",
            _slotInfo.productName,
            "  Disfruta tu       ",
            "  producto!         ");
}

void VmFsm::onEnterVendError() {
    if (_stockReserved) {
        _stockReserved = false;
        _data.releaseStock(_selectedSlot);
    }

    _refundMode     = true;
    _refundCentavos = _insertedCentavos;
    _motorTimer     = millis();

    bool toCard = false;
    if (_paymentMethod == 1u && _refundCentavos > 0u) {
        if (_rfid.creditBalance(_refundCentavos) == RfidWriteResult::OK) {
            _rfidBalanceAfter += _refundCentavos;
            _refundCentavos = 0u;
            toCard = true;
        }
    }

    if (total > 0u) {
        Serial.println(F("CHANGE_ERROR_CALC_NOT_POSSIBLE"));
        return false;
    if (toCard) {
        display("  Falla en proceso  ",
                "  Saldo devuelto a  ",
                "  su tarjeta RFID   ",
                "                    ");
    } else {
        display("  Falla en proceso  ",
                "  Devolviendo       ",
                "  monedas...        ",
                "                    ");
    }
}

void VmFsm::onEnterCalcChange() {
    _motorTimer = millis();

    memset(&_changeResult, 0, sizeof(_changeResult));

    uint32_t cambio;
    if (_refundMode) {
        cambio = _refundCentavos;
    } else {
        cambio = (_insertedCentavos > _slotInfo.priceCentavos)
                     ? (_insertedCentavos - _slotInfo.priceCentavos) : 0u;
    }

    if (cambio == 0u) {
        enterState(FsmState::S12_FINISH_SCREEN);
        return;
    }

    bool ok = _refundMode
                  ? _changeCalc.calculate(_refundCentavos, 0u, _data, _changeResult)
                  : _changeCalc.calculate(_insertedCentavos,
                                          _slotInfo.priceCentavos,
                                          _data, _changeResult);

    if (!ok) {
        _owedCentavos += cambio;

        char amount[16], l3[VM_DISPLAY_LINE_LEN];
        formatMoney(cambio, amount, sizeof(amount));
        snprintf(l3, sizeof(l3), "  Adeudo: %-10s", amount);

        showMessage("  No hay cambio     ",
                    "  Avise al tecnico  ",
                    l3,
                    "  [A] Continuar     ",
                    FsmState::S2_STANDBY);
        return;
    }

    enterState(FsmState::S12_FINISH_SCREEN);
}

void VmFsm::onEnterFinishScreen() {
    _carousel.clearScreens();
    _changeSlides = 0;

    if (_refundMode) {
        _carousel.addScreen(finRefundTrampoline);
    } else {
        _carousel.addScreen(finSuccessTrampoline);
    }

    if (!_refundMode && _paymentMethod == 1u) {
        _carousel.addScreen(finRfidTrampoline);
        _changeSlides = 1;
    } else if (!_refundMode || _refundCentavos > 0u) {
        // Venta en efectivo, o reembolso en monedas
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
    KeyMode mode = KeyMode::PAYMENT;
    if (_state == FsmState::S2_STANDBY)         mode = KeyMode::STANDBY;
    if (_state == FsmState::S13_MESSAGE_PROMPT) mode = KeyMode::PROMPT;
    if (_state == FsmState::S5_WAIT_CASH)       mode = KeyMode::CASH;

    KeyAction action = _keypad.interpret(key, mode);

    switch (_state) {
        case FsmState::S2_STANDBY:         processKeyStandby(action);       break;
        case FsmState::S3_SELECT_CHANNEL:  processKeySelectChannel(action); break;
        case FsmState::S4_SELECT_PAYMENT:  processKeySelectPayment(action); break;
        case FsmState::S5_WAIT_CASH:       processKeyWaitCash(action);      break;
        case FsmState::S13_MESSAGE_PROMPT: processKeyMessagePrompt(action); break;
        case FsmState::S6_WAIT_RFID:
            if (action == KeyAction::CANCEL_ABORT ||
                action == KeyAction::CHOOSE_RFID) {
                enterState(FsmState::S2_STANDBY);
            }
            break;
        default:
            break;
    }
}

void VmFsm::processKeyStandby(KeyAction a) {
    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_4) {
        _selectedSlot = (uint8_t)a - (uint8_t)KeyAction::SELECT_1 + 1u;
        enterState(FsmState::S3_SELECT_CHANNEL);
    }
}

void VmFsm::processKeySelectChannel(KeyAction a) {
    if (a == KeyAction::CHOOSE_CASH) {
        enterState(FsmState::S4_SELECT_PAYMENT);
    } else if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_STANDBY);
    }
}

void VmFsm::processKeySelectPayment(KeyAction a) {
    if (a == KeyAction::CHOOSE_CASH) {
        enterState(FsmState::S5_WAIT_CASH);
    } else if (a == KeyAction::CHOOSE_RFID) {
        if (_rfid.isAvailable()) {
            enterState(FsmState::S6_WAIT_RFID);
        } else {
            showMessage("  RFID no           ",
                        "  disponible.       ",
                        "  Use efectivo.     ",
                        "  [A] Continuar     ",
                        FsmState::S4_SELECT_PAYMENT);
        }
    } else if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_STANDBY);
    }
}

void VmFsm::processKeyWaitCash(KeyAction a) {
    if (a == KeyAction::CANCEL_ABORT) {
        if (_insertedCentavos > 0u) {
            startRefund(_insertedCentavos);
        } else {
            enterState(FsmState::S2_STANDBY);
        }
        return;
    }

    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_4) {
        uint32_t denom = _keypad.coinActionToCentavos(a);
        if (denom == 0u) return;

        if (!_data.addCoins(denom, 1u)) return;

        _insertedCentavos += denom;
        resetInactivityTimer();

        if (_insertedCentavos >= _slotInfo.priceCentavos) {
            enterState(FsmState::S7_PREPARING_VEND);
            return;
        }

        renderCashScreen();
    }
}

void VmFsm::buildReposoScreen(uint8_t idx,
                               char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);

    if (idx >= 4u) {
        char amount[16];
        formatMoney(_owedCentavos, amount, sizeof(amount));
        setLinef(lines[0], "  Cambio pendiente  ");
        setLinef(lines[1], "  Monto: %s", amount);
        setLinef(lines[2], "  Avise al tecnico  ");
        setLinef(lines[3], "");
        return;
    }

    uint8_t slotNum = idx + 1u;
    SlotInfo info;
    memset(&info, 0, sizeof(info));

    if (_data.getSlot(slotNum, info)) {
        char priceBuf[16];
        formatMoney(info.priceCentavos, priceBuf, sizeof(priceBuf));

        setLinef(lines[0], "  Selecciona Canal  ");
        setLinef(lines[1], "%d) %s", (int)slotNum, info.productName);
        setLinef(lines[2], "  Precio: %s", priceBuf);
        setLinef(lines[3], "  Stock : %lu", (unsigned long)info.stock);
    }
}

void VmFsm::buildFinSuccessScreen(uint8_t /*idx*/,
                                   char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    setLinef(lines[0], "  Compra Exitosa!");
    setLinef(lines[1], "  %s", _slotInfo.productName);
    setLinef(lines[2], "  Disfruta tu");
    setLinef(lines[3], "  producto!");
}

void VmFsm::buildFinRfidScreen(uint8_t /*idx*/,
                                char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    char priceBuf[16], saldoBuf[16];
    formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));
    formatMoney(_rfidBalanceAfter,       saldoBuf, sizeof(saldoBuf));
    setLinef(lines[0], " Cobrado con RFID");
    setLinef(lines[1], " Monto: %s", priceBuf);
    setLinef(lines[2], " Saldo rest: %s", saldoBuf);
    setLinef(lines[3], "  Gracias!");
}

void VmFsm::buildFinRefundScreen(uint8_t /*idx*/,
                                  char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    setLinef(lines[0], " Operacion cancelada");

    if (_refundCentavos > 0u) {            // reembolso en monedas
        char amount[16];
        formatMoney(_refundCentavos, amount, sizeof(amount));
        setLinef(lines[1], " Reembolso: %s", amount);
        setLinef(lines[2], " Recoge tus monedas");
    } else {                               // reembolso a tarjeta RFID
        char saldoBuf[16];
        formatMoney(_rfidBalanceAfter, saldoBuf, sizeof(saldoBuf));
        setLinef(lines[1], " Saldo devuelto");
        setLinef(lines[2], " Saldo: %s", saldoBuf);
    }
    setLinef(lines[3], "  Disculpe la falla");
}

void VmFsm::buildChangeScreen(uint8_t idx,
                               char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_COUNT * LCD_LINE_LEN);

    // idx=1 es la primera slide de cambio (idx=0 es exito / cancelacion)
    uint8_t denomIdx = (idx > 0u) ? (idx - 1u) : 0u;
    if (denomIdx >= _changeSlides) return;

    uint32_t denom = _changeDenoms[denomIdx];
    uint32_t qty   = _changeQtys[denomIdx];

    char denomBuf[16];
    formatMoney(denom, denomBuf, sizeof(denomBuf));

    setLinef(lines[0], _refundMode ? "  Tu reembolso:" : "  Tu cambio:");
    setLinef(lines[1], "  Denominacion:");
    setLinef(lines[2], "  %-6s x %lu pza%s",
             denomBuf, (unsigned long)qty, (qty == 1u ? "" : "s"));
    setLinef(lines[3], "  Recoge tu dinero");
}

void VmFsm::renderCashScreen() {
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

void VmFsm::showMessage(const char* l1, const char* l2, const char* l3, const char* l4, FsmState next) {
    snprintf(_promptLines[0], VM_DISPLAY_LINE_LEN, "%-20.20s", l1 ? l1 : "");
    snprintf(_promptLines[1], VM_DISPLAY_LINE_LEN, "%-20.20s", l2 ? l2 : "");
    snprintf(_promptLines[2], VM_DISPLAY_LINE_LEN, "%-20.20s", l3 ? l3 : "");
    snprintf(_promptLines[3], VM_DISPLAY_LINE_LEN, "%-20.20s", l4 ? l4 : "");
    _promptNextState = next;
    enterState(FsmState::S13_MESSAGE_PROMPT);
}

void VmFsm::onEnterMessagePrompt() {
    display(_promptLines[0], _promptLines[1], _promptLines[2], _promptLines[3]);
    resetInactivityTimer();
}

void VmFsm::processKeyMessagePrompt(KeyAction a) {
    if (a == KeyAction::CONTINUE) {
        enterState(_promptNextState);
    }
}

void VmFsm::clearOwed() {
    _owedCentavos = 0u;
}

uint32_t VmFsm::owedCentavos() const {
    return _owedCentavos;
}
