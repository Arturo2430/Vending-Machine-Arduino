#include "vm_motor_controller.h"

VmMotorController::VmMotorController()
    : _pca(VM_PCA9685_ADDRESS),
      _state(IDLE),
      _channel(0),
      _startTime(0),
      _result(-1) {
}

void VmMotorController::begin() {
    Wire.begin();

    _pca.begin();
    _pca.setPWMFreq(VM_PCA9685_PWM_HZ);

    stopAll();

    _state = IDLE;
    _channel = 0;
    _startTime = 0;
    _result = -1;
}

bool VmMotorController::start(uint8_t channel) {
    if (channel < 1u || channel > VM_MOTOR_COUNT) {
        return false;
    }

    if (_state == RUNNING) {
        return false;
    }

    if (digitalRead(VM_PIN_BARRIER) == VM_BARRIER_OCCUPIED_LEVEL) {
        return false;
    }

    stopAll();

    _channel = channel;
    _startTime = millis();
    _result = -1;
    _state = RUNNING;

    setMotorPWM(channel, VM_MOTOR_START_PWM, true);
    return true;
}

void VmMotorController::poll() {
    if (_state != RUNNING) {
        return;
    }

    if (digitalRead(VM_PIN_BARRIER) == VM_BARRIER_OCCUPIED_LEVEL) {
        stopAll();

        _result = 0; // VM_RESULT_DELIVERED
        _state = FINISHED;
        return;
    }

    if ((uint32_t)(millis() - _startTime) >= VM_MOTOR_MAX_TIME_MS) {
        stopAll();

        _result = 2; // VM_RESULT_UNCERTAIN
        _state = UNCERTAIN;
    }
}
void VmMotorController::stop() {
    stopAll();

    if (_state == RUNNING) {
        _result = 2;
        _state = UNCERTAIN;
    }
}

bool VmMotorController::isBusy() const {
    return _state == RUNNING;
}

int VmMotorController::consumeResult() {
    if (_state == RUNNING) {
        return -1;
    }

    if (_state == FINISHED || _state == UNCERTAIN) {
        int result = _result;

        _state = IDLE;
        _result = -1;
        _channel = 0;

        return result;
    }

    return -1;
}

void VmMotorController::setMotorPWM(uint8_t channel,
                                    uint16_t pwm,
                                    bool forward) {
    if (channel < 1u || channel > VM_MOTOR_COUNT) {
        return;
    }

    uint8_t index = channel - 1u;
    bool direction = forward;

    if (VM_MOTOR_DIRECTION_INVERTED[index]) {
        direction = !direction;
    }

    uint8_t first = VM_MOTOR_IN_A[index];
    uint8_t second = VM_MOTOR_IN_B[index];

    if (direction) {
        _pca.setPWM(first, 0u, pwm);
        _pca.setPWM(second, 0u, 0u);
    } else {
        _pca.setPWM(first, 0u, 0u);
        _pca.setPWM(second, 0u, pwm);
    }
}

void VmMotorController::stopAll() {
    for (uint8_t channel = 1u;
         channel <= VM_MOTOR_COUNT;
         channel++) {
        uint8_t index = channel - 1u;

        _pca.setPWM(VM_MOTOR_IN_A[index], 0u, 0u);
        _pca.setPWM(VM_MOTOR_IN_B[index], 0u, 0u);
    }
}