#ifndef VM_MOTOR_CONTROLLER_H
#define VM_MOTOR_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "vm_motor_config.h"
#include "vm_board_config.h"
// Controlador de motores usando PCA9685
class VmMotorController {
public:
    VmMotorController();

    void begin();
    bool start(uint8_t channel); // Inicia un motor, false si la barrera esta bloqueada
    void poll();                 // Metodo no bloqueante para vigilar la barrera
    void stop();                 // Detiene el motor actual
    bool isBusy() const;
    int consumeResult();         // Devuelve el estado de dispensado y resetea

private:
    enum State {
        IDLE,
        RUNNING,
        FINISHED,
        UNCERTAIN
    };

    Adafruit_PWMServoDriver _pca;
    State _state;
    uint8_t _channel;
    uint32_t _startTime;
    int _result;                 // Almacena vm_dispense_result_t

    void setMotorPWM(uint8_t channel, uint16_t pwm, bool forward);
    void stopAll();
};

#endif