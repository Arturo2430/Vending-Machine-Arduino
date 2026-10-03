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

// Puntero a funcion de impresion en LCD
typedef void (*DisplayFn)(const char* line1, const char* line2,
                          const char* line3, const char* line4);

// Tiempo del aviso de falla antes de reembolsar
#ifndef VM_ERROR_DISPLAY_MS
#define VM_ERROR_DISPLAY_MS 2500u
#endif

// Estados de la maquina expendedora
enum class FsmState : uint8_t {
    S0_START           = 0,
    S1_INTERNAL_ERROR  = 1,
    S2_STANDBY         = 2,
    S3_SELECT_CHANNEL  = 3,
    S4_SELECT_PAYMENT  = 4,
    S5_WAIT_CASH       = 5,
    S6_WAIT_RFID       = 6,
    S7_PREPARING_VEND  = 7,
    S8_DISPENSING      = 8,
    S9_CONFIRMED       = 9,
    S10_VEND_ERROR     = 10,
    S11_CALC_CHANGE    = 11,
    S12_FINISH_SCREEN  = 12,
    S13_MESSAGE_PROMPT = 13,
    S14_REFUND_RFID    = 14,
    S15_REFUND_CALC    = 15,
    S16_REFUND_FINISH  = 16
};

// Logica principal de estados de la maquina
class VmFsm {
public:
    VmFsm(VmEepromData& data, VmMotorController& motor, VmRfid& rfid,
          DisplayFn displayFn);

    void begin();
    void update();
    void handleKey(char key);
    FsmState currentState() const { return _state; }

    // Manejo de saldos adeudados por falta de cambio
    uint32_t owedCentavos() const { return _owedCentavos; }
    void     clearOwed()          { _owedCentavos = 0u; }

    // Generadores de vistas para el carrusel
    void buildReposoScreen    (uint8_t idx, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    void buildFinSuccessScreen(uint8_t idx, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    void buildFinRfidScreen   (uint8_t idx, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    void buildFinRefundScreen (uint8_t idx, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    void buildChangeScreen    (uint8_t idx, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);

private:
    VmEepromData&      _data;
    VmMotorController& _motor;
    VmRfid&            _rfid;
    DisplayFn          _displayFn;

    VmKeypad           _keypad;
    VmCarousel         _carousel;
    VmChangeCalculator _changeCalc;

    FsmState _state;

    // Estado local de la transaccion
    uint8_t      _selectedSlot;
    SlotInfo     _slotInfo;
    uint32_t     _insertedCentavos;
    bool         _stockReserved;
    uint8_t      _pendingResult;
    uint8_t      _paymentMethod;
    uint32_t     _rfidBalanceAfter;
    ChangeResult _changeResult;

    // Estado persistente
    uint32_t _owedCentavos;

    // Monedas devueltas para impresion
    uint32_t _changeDenoms[4];
    uint32_t _changeQtys[4];
    uint8_t  _changeSlides;

    // Buffer de mensajes genericos
    char     _promptLines[4][VM_DISPLAY_LINE_LEN];
    FsmState _promptNextState;

    unsigned long _inactivityTimer;
    unsigned long _motorTimer;

    void enterState(FsmState next);

    // Eventos al entrar a un estado
    void onEnterStart();
    void onEnterInternalError();
    void onEnterStandby();
    void onEnterSelectChannel(uint8_t slot);
    void onEnterSelectPayment();
    void onEnterWaitCash();
    void onEnterWaitRfid();
    void onEnterPreparingVend();
    void onEnterDispensing();
    void onEnterConfirmed();
    void onEnterVendError();
    void onEnterCalcChange();
    void onEnterFinishScreen();
    void onEnterMessagePrompt();
    void onEnterRefundRfid();
    void onEnterRefundCalc();
    void onEnterRefundFinish();

    // Eventos de teclado por estado
    void processKeyStandby(KeyAction a);
    void processKeySelectChannel(KeyAction a);
    void processKeySelectPayment(KeyAction a);
    void processKeyWaitCash(KeyAction a);
    void processKeyMessagePrompt(KeyAction a);

    // Utilidades graficas
    void renderCashScreen(const char* title = "Monedas  [*]Cancelar");
    void showMessage(const char* l1, const char* l2, const char* l3,
                     const char* l4, FsmState next);
    void addChangeSlides();
    void display(const char* l1, const char* l2,
                 const char* l3, const char* l4);

    void resetInactivityTimer() { _inactivityTimer = millis(); }
    bool inactivityExpired() const {
        return (uint32_t)(millis() - _inactivityTimer) >= VM_TIMEOUT_INACTIVITY_MS;
    }
    bool motorTimerExpired() const {
        return (uint32_t)(millis() - _motorTimer) >= VM_TIMEOUT_MOTOR_MS;
    }
};

#endif // VM_FSM_H
