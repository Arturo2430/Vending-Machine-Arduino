#include "vm_motor_controller.h"

VmMotorController::VmMotorController()
    : _pca(VM_PCA9685_ADDRESS),
      _state(IDLE),
      _channel(0),
      _startTime(0),
      _result(-1),
      _ultimaMedicion(0),
      _lecturasConsecutivas(0) {
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
    _ultimaMedicion = 0;
    _lecturasConsecutivas = 0;
}

bool VmMotorController::medirDistancia(float& cm) {
    digitalWrite(VM_PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(VM_PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(VM_PIN_TRIG, LOW);
    
    unsigned long duracion = pulseIn(VM_PIN_ECHO, HIGH, 30000UL); // 30ms timeout
    if (duracion == 0) return false;
    
    cm = duracion * 0.0343f / 2.0f;
    return cm >= 2.0f && cm <= 400.0f;
}

bool VmMotorController::start(uint8_t channel) {
    if (channel < 1u || channel > VM_MOTOR_COUNT) {
        return false;
    }

    if (_state == RUNNING) {
        return false;
    }

    // Verificar que la bandeja esté libre antes de arrancar
    float cm = 0;
    if (medirDistancia(cm)) {
        if (cm <= VM_ULTRASONIC_THRESHOLD_CM) {
            return false; // Hay algo en la bandeja
        }
    }

    stopAll();

    _channel = channel;
    _startTime = millis();
    _ultimaMedicion = millis();
    _lecturasConsecutivas = 0;
    _result = -1;
    _state = RUNNING;

    // Enviar FULL ON
    setMotorPWM(channel, 4096u, true);
    return true;
}

void VmMotorController::poll() {
    if (_state != RUNNING) {
        return;
    }

    // Intervalo de lectura para no saturar el sensor
    if (millis() - _ultimaMedicion >= 70) {
        _ultimaMedicion = millis();
        float cm = 0;
        if (medirDistancia(cm)) {
            if (cm <= VM_ULTRASONIC_THRESHOLD_CM) {
                _lecturasConsecutivas++;
                if (_lecturasConsecutivas >= 1) {
                    stopAll();
                    _result = 0; // VM_RESULT_DELIVERED
                    _state = FINISHED;
                    return;
                }
            } else {
                _lecturasConsecutivas = 0;
            }
        }
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
        if (pwm >= 4096u) {
            _pca.setPWM(first, 4096u, 0u); // FULL ON
        } else {
            _pca.setPWM(first, 0u, pwm);
        }
        _pca.setPWM(second, 0u, 4096u); // FULL OFF
    } else {
        _pca.setPWM(first, 0u, 4096u); // FULL OFF
        if (pwm >= 4096u) {
            _pca.setPWM(second, 4096u, 0u); // FULL ON
        } else {
            _pca.setPWM(second, 0u, pwm);
        }
    }
}

void VmMotorController::stopAll() {
    for (uint8_t channel = 1u;
         channel <= VM_MOTOR_COUNT;
         channel++) {
        uint8_t index = channel - 1u;

        // FULL OFF en ambas patas
        _pca.setPWM(VM_MOTOR_IN_A[index], 0u, 4096u);
        _pca.setPWM(VM_MOTOR_IN_B[index], 0u, 4096u);
    }
}