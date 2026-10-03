#ifndef VM_MOTOR_CONFIG_H
#define VM_MOTOR_CONFIG_H

#include <stdint.h>

// Configuracion de actuadores
#define VM_PCA9685_ADDRESS 0x40u   // Direccion I2C del modulo
#define VM_PCA9685_PWM_HZ 1000u    // Frecuencia de la senal
#define VM_MOTOR_COUNT 4u          // Cantidad de motores habilitados
#define VM_MOTOR_MAX_PWM 4095u     // Resolucion PWM a 12 bits
#define VM_MOTOR_DEFAULT_PWM 2800u // Velocidad nominal
#define VM_MOTOR_START_PWM 3200u   // Velocidad de arranque inicial
#define VM_MOTOR_MAX_TIME_MS 5000u // Tiempo limite de operacion

static const uint8_t VM_MOTOR_IN_A[VM_MOTOR_COUNT] = {
    0u, 2u, 4u, 6u // Pines de control A
};

static const uint8_t VM_MOTOR_IN_B[VM_MOTOR_COUNT] = {
    1u, 3u, 5u, 7u // Pines de control B
};

static const bool VM_MOTOR_DIRECTION_INVERTED[VM_MOTOR_COUNT] = {
    false, false, false, false // indica si la direccion del motor es invertida (true va hacia la izquierda, false va hacia la derecha)
};

#endif