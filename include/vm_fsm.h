/**
 * @file vm_fsm.h
 * @brief Máquina de estados de la máquina expendedora (Arduino Mega 2560).
 *
 * Implementa la lógica de venta: selección de producto, pago con efectivo
 * o tarjeta RFID, despacho mediante motores DC y cálculo de cambio.
 *
 * Estados (S0 → S12):
 *   S0  ARRANQUE      – Inicialización del sistema
 *   S1  FALLA_INTERNA – Falla crítica de EEPROM (sistema detenido)
 *   S2  REPOSO        – Espera de selección de producto (pantalla carrusel)
 *   S3  SEL_CANAL     – Confirmación del producto elegido
 *   S4  SEL_PAGO      – Selección de método de pago (efectivo / RFID)
 *   S5  ESP_EFECTIVO  – Acumulación de monedas hasta alcanzar el precio
 *   S6  ESP_RFID      – Lectura y cobro en tarjeta RFID
 *   S7  RESERVADA     – Verificación de stock y condiciones físicas
 *   S8  DISPENSANDO   – Motor activo: espera confirmación de barrera óptica
 *   S9  CONFIRMADA    – Entrega exitosa confirmada
 *   S10 FALLA_DISP    – Error de despacho con rollback de inventario
 *   S11 CALC_CAMBIO   – Cálculo de monedas a devolver
 *   S12 PANTALLA_FIN  – Resumen de la compra y carrusel final
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

/* Firma de la función de display inyectada desde main.cpp. */
typedef void (*DisplayFn)(const char* line1, const char* line2,
                          const char* line3, const char* line4);

/* Identificadores de los 13 estados de la FSM. */
enum class FsmState : uint8_t {
    S0_ARRANQUE      = 0,
    S1_FALLA_INTERNA = 1,
    S2_REPOSO        = 2,
    S3_SEL_CANAL     = 3,
    S4_SEL_PAGO      = 4,
    S5_ESP_EFECTIVO  = 5,
    S6_ESP_RFID      = 6,
    S7_RESERVADA     = 7,
    S8_DISPENSANDO   = 8,
    S9_CONFIRMADA    = 9,
    S10_FALLA_DISP   = 10,
    S11_CALC_CAMBIO  = 11,
    S12_PANTALLA_FIN = 12
};

class VmFsm {
public:
    VmFsm(VmEepromData& data, VmMotorController& motor, VmRfid& rfid,
          DisplayFn displayFn);

    void begin();
    void update();
    void handleKey(char key);
    FsmState currentState() const { return _state; }

    // Accesibles para los trampolines del carrusel
    void buildReposoScreen(uint8_t index, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    void buildFinScreen(uint8_t index, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);

private:
    // ---- Dependencias inyectadas ----
    VmEepromData&      _data;
    VmMotorController& _motor;
    VmRfid&            _rfid;
    DisplayFn          _displayFn;

    // ---- Subsistemas propios ----
    VmKeypad           _keypad;
    VmCarousel         _carousel;
    VmChangeCalculator _changeCalc;

    // ---- Estado de la FSM ----
    FsmState _state;

    // ---- Datos de la transacción en curso ----
    uint8_t      _selectedSlot;
    SlotInfo     _slotInfo;
    uint32_t     _insertedCentavos;
    bool         _stockReserved;
    uint8_t      _pendingResult;    // vm_dispense_result_t
    uint8_t      _paymentMethod;    // 0 = efectivo, 1 = RFID
    ChangeResult _changeResult;

    // ---- Temporizadores (ms) ----
    unsigned long _inactivityTimer;
    unsigned long _motorTimer;

    // ---- Sensor de puerta (debounce) ----
    bool          _doorSampled;
    bool          _doorStable;
    unsigned long _doorLastChangeMs;

    // ---- Transiciones de estado ----
    void enterState(FsmState next);

    // ---- Funciones de entrada a cada estado (S0 → S12) ----
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

    // ---- Procesadores de teclado por estado ----
    void processKeyReposo(KeyAction a);
    void processKeySelCanal(KeyAction a);
    void processKeySelPago(KeyAction a);
    void processKeyEspEfectivo(KeyAction a);

    // ---- Utilidades ----
    void updateDoor();
    void renderEfectivoScreen();
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