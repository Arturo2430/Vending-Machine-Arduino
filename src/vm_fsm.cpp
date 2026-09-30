/**
 * @file vm_fsm.cpp
 * @brief Implementación de la máquina de estados de la máquina expendedora.
 *
 * Organización del archivo (lectura lineal de arriba a abajo):
 *   1. Trampolines estáticos del carrusel y utilidad formatMoney
 *   2. Constructor y begin()
 *   3. update()  – ciclo periódico de la FSM (S0 → S12)
 *   4. enterState() – dispatcher de funciones de entrada
 *   5. onEnter*() – acciones al entrar a cada estado (S0 → S12)
 *   6. handleKey() y processKey*() – manejo de teclado por estado
 *   7. buildReposoScreen / buildFinScreen – contenido del carrusel
 *   8. updateDoor() – filtro de rebotes del sensor de puerta
 */

#include <string.h>
#include "vm_fsm.h"
#include "vm_change_calculator.h"

// ===========================================================================
// 1. Trampolines del carrusel y utilidades
// ===========================================================================

// El carrusel requiere funciones libres; usamos un puntero estático al FSM
// activo para delegar la construcción de pantallas.
static VmFsm* s_fsm = NULL;

static void reposoScreenTrampoline(uint8_t index,
                                    char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildReposoScreen(index, lines);
}

static void finScreenTrampoline(uint8_t index,
                                char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    if (s_fsm) s_fsm->buildFinScreen(index, lines);
}

// Formatea centavos como "$X.XX" en el buffer dado.
static void formatMoney(uint32_t centavos, char* buf, size_t size) {
    uint32_t pesos = centavos / 100u;
    uint32_t cents = centavos % 100u;
    snprintf(buf, size, "$%lu.%02lu", (unsigned long)pesos, (unsigned long)cents);
}

// ===========================================================================
// 2. Constructor y begin()
// ===========================================================================

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
      _inactivityTimer(0u),
      _motorTimer(0u),
      _doorSampled(false),
      _doorStable(false),
      _doorLastChangeMs(0u)
{
    memset(&_slotInfo,     0, sizeof(_slotInfo));
    memset(&_changeResult, 0, sizeof(_changeResult));
    s_fsm = this;
}

void VmFsm::begin() {
    bool dataOk = _data.begin();

    if (!dataOk) {
        enterState(FsmState::S1_FALLA_INTERNA);
        return;
    }

    pinMode(VM_PIN_DOOR,    INPUT_PULLUP);
    pinMode(VM_PIN_BARRIER, INPUT_PULLUP);

    _doorSampled      = (digitalRead(VM_PIN_DOOR) == VM_DOOR_CLOSED_LEVEL);
    _doorStable       = _doorSampled;
    _doorLastChangeMs = millis();

    enterState(FsmState::S0_ARRANQUE);
}

void VmFsm::display(const char* l1, const char* l2,
                    const char* l3, const char* l4) {
    if (_displayFn) _displayFn(l1, l2, l3, l4);
}

// ===========================================================================
// 3. update() – ciclo periódico (llamado desde loop())
// ===========================================================================

void VmFsm::update() {
    _motor.poll();
    _carousel.update();
    updateDoor();

    switch (_state) {

        // S0: espera a que los datos estén listos → pasa a reposo
        case FsmState::S0_ARRANQUE: {
            enterState(FsmState::S2_REPOSO);
            break;
        }

        // S2-S4-S5: timeout de inactividad → regresa a reposo
        case FsmState::S3_SEL_CANAL:
        case FsmState::S4_SEL_PAGO:
        case FsmState::S5_ESP_EFECTIVO: {
            if (inactivityExpired()) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }

        // S6: sondeo no bloqueante de RFID + timeout de inactividad
        case FsmState::S6_ESP_RFID: {
            if (inactivityExpired()) {
                enterState(FsmState::S2_REPOSO);
                break;
            }

            RfidReadResult rr = _rfid.poll();

            if (rr == RfidReadResult::OK) {
                uint32_t saldo = _rfid.lastBalance();

                if (saldo >= _slotInfo.priceCentavos) {
                    // Saldo suficiente: descontar y pasar a reservar
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
                    // Saldo insuficiente: informar y regresar a reposo
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
            // RfidReadResult::NONE: sin tarjeta, seguir esperando
            break;
        }

        // S7: si no hay stock, regresar a reposo tras 1.5 s
        case FsmState::S7_RESERVADA: {
            if (!_stockReserved &&
                (uint32_t)(millis() - _motorTimer) >= 1500u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }

        // S8: supervisar motor → confirmada o falla
        case FsmState::S8_DISPENSANDO: {
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
        }

        // S9: tras 1 s de pantalla de éxito → calcular cambio o pantalla fin
        case FsmState::S9_CONFIRMADA: {
            if ((uint32_t)(millis() - _motorTimer) >= 1000u) {
                if (_paymentMethod == 1u) {
                    // RFID: pago exacto, sin cambio
                    memset(&_changeResult, 0, sizeof(_changeResult));
                    enterState(FsmState::S12_PANTALLA_FIN);
                } else {
                    enterState(FsmState::S11_CALC_CAMBIO);
                }
            }
            break;
        }

        // S10: tras 3 s de pantalla de error → regresar a reposo
        case FsmState::S10_FALLA_DISP: {
            if ((uint32_t)(millis() - _motorTimer) >= 3000u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }

        // S11: tras 5 s de pantalla de cambio → regresar a reposo
        case FsmState::S11_CALC_CAMBIO: {
            if ((uint32_t)(millis() - _motorTimer) >= 5000u) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }

        // S12: tras 2 ciclos de carrusel → regresar a reposo
        case FsmState::S12_PANTALLA_FIN: {
            if ((uint32_t)(millis() - _inactivityTimer) >=
                (VM_CAROUSEL_INTERVAL_MS * 2u + 1000u)) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }

        default:
            break;
    }
}

// ===========================================================================
// 4. enterState() – dispatcher
// ===========================================================================

void VmFsm::enterState(FsmState next) {
    _state = next;

    switch (next) {
        case FsmState::S0_ARRANQUE:      onEnterArranque();             break;
        case FsmState::S1_FALLA_INTERNA: onEnterFallaInterna();         break;
        case FsmState::S2_REPOSO:        onEnterReposo();               break;
        case FsmState::S3_SEL_CANAL:     onEnterSelCanal(_selectedSlot); break;
        case FsmState::S4_SEL_PAGO:      onEnterSelPago();              break;
        case FsmState::S5_ESP_EFECTIVO:  onEnterEspEfectivo();          break;
        case FsmState::S6_ESP_RFID:      onEnterEspRfid();              break;
        case FsmState::S7_RESERVADA:     onEnterReservada();            break;
        case FsmState::S8_DISPENSANDO:   onEnterDispensando();          break;
        case FsmState::S9_CONFIRMADA:    onEnterConfirmada();           break;
        case FsmState::S10_FALLA_DISP:   onEnterFallaDisp();            break;
        case FsmState::S11_CALC_CAMBIO:  onEnterCalcCambio();           break;
        case FsmState::S12_PANTALLA_FIN: onEnterPantallaFin();          break;
        default: break;
    }
}

// ===========================================================================
// 5. onEnter*() – acciones al entrar a cada estado (S0 → S12)
// ===========================================================================

// S0 – Arranque: mensaje inicial mientras los datos se inicializan
void VmFsm::onEnterArranque() {
    display("   Iniciando...    ",
            "   Maquina         ",
            "   Expendedora     ",
            "                   ");
}

// S1 – Falla interna: error crítico de EEPROM, sistema detenido
void VmFsm::onEnterFallaInterna() {
    display("  ERROR INTERNO    ",
            "  EEPROM no disp.  ",
            "  Reinicie o llame ",
            "  al tecnico.      ");
}

// S2 – Reposo: mostrar catálogo en carrusel y esperar selección
void VmFsm::onEnterReposo() {
    _carousel.clearScreens();
    _carousel.addScreen(reposoScreenTrampoline);
    _carousel.begin();
    resetInactivityTimer();
}

// S3 – Selección de canal: mostrar producto y precio elegidos
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

// S4 – Selección de pago: elegir entre efectivo o tarjeta RFID
void VmFsm::onEnterSelPago() {
    display("  Metodo de pago   ",
            "  [A] Efectivo     ",
            "  [B] Tarjeta RFID ",
            "  [*] Cancelar     ");
    resetInactivityTimer();
}

// S5 – Espera de efectivo: acumular monedas hasta alcanzar el precio
void VmFsm::onEnterEspEfectivo() {
    _paymentMethod    = 0u;
    _insertedCentavos = 0u;
    resetInactivityTimer();
    renderEfectivoScreen();
}

// S6 – Espera de RFID: acercar tarjeta al lector
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

// S7 – Reservada: verificar stock y condiciones físicas antes de girar motor
void VmFsm::onEnterReservada() {
    resetInactivityTimer();
    _stockReserved = false;

    if (!_data.reserveStock(_selectedSlot)) {
        display("  Sin stock         ",
                "  Seleccione otro   ",
                "  canal.            ",
                "                    ");
        _motorTimer = millis(); // regresa a reposo en 1.5 s (ver update)
        return;
    }

    _stockReserved = true;
    _data.beginOrder(_selectedSlot);

    // Verificar puerta cerrada y barrera libre antes de arrancar el motor
    if (!_doorStable || !_motor.start(_selectedSlot)) {
        _pendingResult = VM_RESULT_REJECTED_BEFORE_MOTION;
        enterState(FsmState::S10_FALLA_DISP);
        return;
    }

    enterState(FsmState::S8_DISPENSANDO);
}

// S8 – Dispensando: motor girando, esperando señal de barrera óptica
void VmFsm::onEnterDispensando() {
    _motorTimer = millis();
    display("  Despachando...    ",
            _slotInfo.productName,
            "  Espere por favor  ",
            "                    ");
}

// S9 – Confirmada: producto entregado exitosamente
void VmFsm::onEnterConfirmada() {
    _stockReserved = false;
    _data.completeOrder(_selectedSlot, VM_RESULT_DELIVERED);
    _motorTimer = millis();

    display("  Compra Exitosa!   ",
            _slotInfo.productName,
            "  Disfruta tu       ",
            "  producto!         ");
    // update() esperará 1 s y luego avanzará a S11 o S12
}

// S10 – Falla de despacho: rollback de inventario y mensaje de error
void VmFsm::onEnterFallaDisp() {
    if (_stockReserved) {
        _stockReserved = false;
        _data.completeOrder(_selectedSlot, _pendingResult);
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

// S11 – Cálculo de cambio: algoritmo greedy sobre caja de monedas
void VmFsm::onEnterCalcCambio() {
    _motorTimer = millis();

    uint32_t cambio = (_insertedCentavos > _slotInfo.priceCentavos)
                          ? (_insertedCentavos - _slotInfo.priceCentavos)
                          : 0u;

    if (cambio == 0u) {
        // Sin cambio: ir directo a pantalla final
        enterState(FsmState::S12_PANTALLA_FIN);
        return;
    }

    memset(&_changeResult, 0, sizeof(_changeResult));

    bool ok = _changeCalc.calculate(_insertedCentavos,
                                    _slotInfo.priceCentavos,
                                    _data,
                                    _changeResult);
    if (!ok) {
        // No hay monedas suficientes para el cambio
        display("  No hay cambio     ",
                "  disponible.       ",
                "  Avisa al tecnico. ",
                "                    ");
        return; // update() esperará 5 s y regresará a reposo
    }

    enterState(FsmState::S12_PANTALLA_FIN);
}

// S12 – Pantalla final: carrusel con resumen de compra y cambio entregado
void VmFsm::onEnterPantallaFin() {
    _carousel.clearScreens();
    _carousel.addScreen(finScreenTrampoline);
    _carousel.begin();
    _inactivityTimer = millis();
}

// ===========================================================================
// 6. handleKey() y processKey*() – manejo de teclado por estado
// ===========================================================================

void VmFsm::handleKey(char key) {
    // Determinar el modo de interpretación según el estado actual
    KeyMode mode = KeyMode::PAGO;
    if (_state == FsmState::S2_REPOSO)       mode = KeyMode::REPOSO;
    if (_state == FsmState::S5_ESP_EFECTIVO) mode = KeyMode::EFECTIVO;

    KeyAction action = _keypad.interpret(key, mode);

    switch (_state) {
        case FsmState::S2_REPOSO:        processKeyReposo(action);      break;
        case FsmState::S3_SEL_CANAL:     processKeySelCanal(action);    break;
        case FsmState::S4_SEL_PAGO:      processKeySelPago(action);     break;
        case FsmState::S5_ESP_EFECTIVO:  processKeyEspEfectivo(action); break;
        case FsmState::S6_ESP_RFID: {
            // Solo 'B' cancela la espera de RFID
            if (action == KeyAction::CANCEL_ABORT ||
                action == KeyAction::CHOOSE_RFID) {
                enterState(FsmState::S2_REPOSO);
            }
            break;
        }
        default:
            break; // S7..S12: no hay entradas de teclado relevantes
    }
}

// S2 – Reposo: teclas 1-4 seleccionan canal
void VmFsm::processKeyReposo(KeyAction a) {
    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_4) {
        _selectedSlot = (uint8_t)a - (uint8_t)KeyAction::SELECT_1 + 1u;
        enterState(FsmState::S3_SEL_CANAL);
    }
}

// S3 – Selección de canal: A confirma, * cancela
void VmFsm::processKeySelCanal(KeyAction a) {
    if (a == KeyAction::CHOOSE_CASH) {
        enterState(FsmState::S4_SEL_PAGO);
    } else if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_REPOSO);
    }
}

// S4 – Selección de pago: A=efectivo, B=RFID, *=cancelar
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

// S5 – Efectivo: cada tecla numérica agrega una denominación de moneda
void VmFsm::processKeyEspEfectivo(KeyAction a) {
    if (a == KeyAction::CANCEL_ABORT) {
        enterState(FsmState::S2_REPOSO);
        return;
    }

    if (a >= KeyAction::SELECT_1 && a <= KeyAction::SELECT_9) {
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

// ===========================================================================
// 7. Construcción de pantallas del carrusel
// ===========================================================================

// Pantalla de reposo: dos productos por subpantalla, rotando de a dos
void VmFsm::buildReposoScreen(uint8_t index,
                               char lines[LCD_LINE_COUNT][LCD_LINE_LEN]) {
    memset(lines, 0, LCD_LINE_LEN * LCD_LINE_COUNT);
    snprintf(lines[0], LCD_LINE_LEN, "  Selecciona canal  ");

    uint8_t start = (index * 2u) % VM_CHANNEL_MAX;
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

// Pantalla final: slide 0 = éxito, slide 1 = desglose de cambio (o RFID)
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
        // Pago con RFID: mostrar monto cobrado y saldo restante
        char priceBuf[16], saldoBuf[16];
        formatMoney(_slotInfo.priceCentavos, priceBuf, sizeof(priceBuf));
        formatMoney(_rfid.lastBalance(),     saldoBuf, sizeof(saldoBuf));
        snprintf(lines[0], LCD_LINE_LEN, " Cobrado con RFID   ");
        snprintf(lines[1], LCD_LINE_LEN, " Monto: %-12s", priceBuf);
        snprintf(lines[2], LCD_LINE_LEN, " Saldo rest: %-7s", saldoBuf);
        snprintf(lines[3], LCD_LINE_LEN, "  Gracias!           ");
        return;
    }

    // Pago en efectivo: desglose de monedas de cambio
    uint32_t cambio = (_insertedCentavos > _slotInfo.priceCentavos)
                          ? (_insertedCentavos - _slotInfo.priceCentavos) : 0u;
    char cambBuf[16];
    formatMoney(cambio, cambBuf, sizeof(cambBuf));

    snprintf(lines[0], LCD_LINE_LEN, " Tu cambio: %-10s", cambBuf);
    snprintf(lines[1], LCD_LINE_LEN, "  $10:%lu   $5:%lu",
             (unsigned long)_changeResult.coin1000,
             (unsigned long)_changeResult.coin500);
    snprintf(lines[2], LCD_LINE_LEN, "  $2:%lu    $1:%lu ",
             (unsigned long)_changeResult.coin200,
             (unsigned long)_changeResult.coin100);
    snprintf(lines[3], LCD_LINE_LEN, "  Recoge tu cambio   ");
}

// ===========================================================================
// 8. Pantalla auxiliar y sensor de puerta
// ===========================================================================

// Muestra el acumulado de monedas y cuánto falta para completar el pago
void VmFsm::renderEfectivoScreen() {
    uint32_t falta = (_slotInfo.priceCentavos > _insertedCentavos)
                         ? (_slotInfo.priceCentavos - _insertedCentavos) : 0u;
    char moneyBuf[16], faltaBuf[16];
    formatMoney(_insertedCentavos, moneyBuf, sizeof(moneyBuf));
    formatMoney(falta,             faltaBuf, sizeof(faltaBuf));

    char l3[VM_DISPLAY_LINE_LEN];
    snprintf(l3, sizeof(l3), " Faltan: %-12s", faltaBuf);

    display("  Inserta monedas   ",
            moneyBuf,
            l3,
            "  [B] Cancelar      ");
}

// Filtro de rebote (debounce) del sensor de puerta
void VmFsm::updateDoor() {
    bool level = (digitalRead(VM_PIN_DOOR) == VM_DOOR_CLOSED_LEVEL);

    if (level != _doorSampled) {
        _doorSampled      = level;
        _doorLastChangeMs = millis();
    } else if ((uint32_t)(millis() - _doorLastChangeMs) >= VM_DOOR_DEBOUNCE_MS) {
        _doorStable = level;
    }
}