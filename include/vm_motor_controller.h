#ifndef VM_MOTOR_CONTROLLER_H
#define VM_MOTOR_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "vm_motor_config.h"
#include "vm_board_config.h"
class VmMotorController {
public:
    VmMotorController();

    void begin();
    bool start(uint8_t channel);
    void poll();
    void stop();
    bool isBusy() const;
    int consumeResult();

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
    int _result;
    unsigned long _ultimaMedicion;
    uint8_t _lecturasConsecutivas;

    void setMotorPWM(uint8_t channel, uint16_t pwm, bool forward);
    void stopAll();
    bool medirDistancia(float& cm);
};

#endif