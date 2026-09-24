/**
 * @file vm_fsm.h
 * @brief Máquina de estados de la venta y administración (portada del ESP32).
 *
 * Este módulo implementa la lógica de negocio completa corriendo en el Mega:
 * selección de producto, pago con efectivo o tarjeta RFID (MIFARE), despacho
 * por motores DC, cálculo de cambio y menú de administración.
 */

#ifndef VM_FSM_H
#define VM_FSM_H

#include <Arduino.h>
#include "vm_types.h"
#include "vm_board_config.h"
#include "vm_eeprom_data.h"
#include "vm_keypad.h"
#include "vm_carousel.h"
#include "vm_change_calculator.h"
#include "vm_motor_controller.h"
#include "vm_rfid.h"

typedef void (*DisplayFn)(const char* line1, const char* line2,
                          const char* line3, const char* line4);

enum class FsmState : uint8_t {
    S0_ARRANQUE       = 0,
    S1_FALLA_INTERNA  = 1,
    S2_REPOSO         = 2,
    S3_SEL_CANAL      = 3,
    S4_SEL_PAGO       = 4,
    S5_ESP_EFECTIVO   = 5,
    S6_ESP_RFID       = 6,
    S7_RESERVADA      = 7,
    S8_DISPENSANDO    = 8,
    S9_CONFIRMADA     = 9,
    S10_FALLA_DISP    = 10,
    S11_CALC_CAMBIO   = 11,
    S12_PANTALLA_FIN  = 12,
    S13_ADMIN_AUTH    = 13,
    S14_ADMIN_CANAL   = 14,
    S15_ADMIN_ACCION  = 15,
    S16_MOD_PRECIO    = 16,
    S17_MOD_STOCK     = 17
};

class VmFsm {
public:
    VmFsm(VmEepromData& data, VmMotorController& motor, VmRfid& rfid,
           DisplayFn displayFn);
    void begin();
    void update();
    void handleKey(char key);
    FsmState currentState() const { return _state; }

private:
    VmEepromData&        _data;
    VmMotorController&   _motor;
    VmRfid&              _rfid;
    DisplayFn            _displayFn;
    VmKeypad             _keypad;
    VmCarousel           _carousel;
    VmChangeCalculator   _changeCalc;

    FsmState _state;
    uint8_t  _mode;                 // vm_mode_t (VENTA/MANTENIMIENTO)
    bool     _dataReady;

    uint8_t  _selectedSlot;
    SlotInfo _slotInfo;
    uint32_t _insertedCentavos;
    bool     _stockReserved;
    uint8_t  _pendingResult;        // vm_dispense_result_t
    uint8_t  _paymentMethod;        // 0=efectivo, 1=RFID
    ChangeResult _changeResult;

    // ---- Administración --------------------------------------------------
    char   _pinBuffer[5];
    uint8_t _pinLen;
    uint8_t _pinFailCount;
    unsigned long _pinLockoutEnd;
    unsigned long _lastAdminActivityMs;
    uint8_t _adminSlot;
    char   _numBuffer[8];
    uint8_t _numLen;

    // ---- Temporizadores --------------------------------------------------
    unsigned long _inactivityTimer;
    unsigned long _motorTimer;

    // ---- Sensor de puerta (debounce) ------------------------------------
    bool _doorSampled;
    bool _doorStable;
    unsigned long _doorLastChangeMs;
    uint8_t  _lastKeyDigit;

    void enterState(FsmState next);

    void onEnterArranque();
    void onEnterFallaInterna();
    void onEnterReposo();
    void onEnterSelCanal(uint8_t slot);
    void onEnterSelPago();
    void onEnterEspEfectivo();
    void onEnterEspRfid();
    void onEnterReservada();
    void onEnterDispensando();
    void onEnterConfirmada();
    void onEnterFallaDisp();
    void onEnterCalcCambio();
    void onEnterPantallaFin();
    void onEnterAdminAuth();
    void onEnterAdminCanal();
    void onEnterAdminAccion();
    void onEnterModPrecio();
    void onEnterModStock();

    void processKeyReposo(KeyAction a);
    void processKeySelCanal(KeyAction a);
    void processKeySelPago(KeyAction a);
    void processKeyEspEfectivo(KeyAction a);
    void processKeyAdminAuth(KeyAction a);
    void processKeyAdminCanal(KeyAction a);
    void processKeyAdminAccion(KeyAction a);
    void processKeyModPrecio(KeyAction a);
    void processKeyModStock(KeyAction a);

    void updateDoor();
    void setMode(uint8_t mode);
    void renderEfectivoScreen();
    void renderPinScreen();
    void renderNumScreen(const char* header);

    void display(const char* l1, const char* l2,
                 const char* l3, const char* l4);

public:
    // Carrousel: accesibles para los trampolines libres de vm_fsm.cpp.
    void buildReposoScreen(uint8_t index, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    void buildFinScreen(uint8_t index, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);

private:
    void resetInactivityTimer() { _inactivityTimer = millis(); }
    bool inactivityExpired() const {
        return (uint32_t)(millis() - _inactivityTimer) >= VM_TIMEOUT_INACTIVITY_MS;
    }
    bool motorTimerExpired() const {
        return (uint32_t)(millis() - _motorTimer) >= VM_TIMEOUT_MOTOR_MS;
    }

    uint32_t numBufferValue() const;
    void clearNumBuffer() { _numLen = 0; memset(_numBuffer, 0, sizeof(_numBuffer)); }
    void appendNumBuffer(uint8_t digit);
    void backspaceNumBuffer();
    void clearPinBuffer() { _pinLen = 0; memset(_pinBuffer, 0, sizeof(_pinBuffer)); }
    void appendPinDigit(uint8_t digit);
    void backspacePinBuffer();
};

#endif // VM_FSM_H