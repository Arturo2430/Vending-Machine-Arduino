#ifndef VM_MOTOR_CONFIG_H
#define VM_MOTOR_CONFIG_H

#include <stdint.h>

// Configuración de los motores
#define VM_PCA9685_ADDRESS       0x40u //direccion I2C por defecto del PCA
#define VM_PCA9685_PWM_HZ        1000u // frecuencia de PWM
#define VM_MOTOR_COUNT           4u    //cantidad de motores a usar
#define VM_MOTOR_MAX_PWM         4095u // valor maximo del PWM (12 bits)
#define VM_MOTOR_DEFAULT_PWM     2800u // valor por defecto del PWM
#define VM_MOTOR_START_PWM       3200u //valor con el que se inicia el motor para que arranque
#define VM_MOTOR_MAX_TIME_MS     5000u // tiempo maximo de ejecucion en milisegundos

static const uint8_t VM_MOTOR_IN_A[VM_MOTOR_COUNT] = {
    0u, 2u, 4u, 6u  //pines de control A de los motores (PWM)
};

static const uint8_t VM_MOTOR_IN_B[VM_MOTOR_COUNT] = {
    1u, 3u, 5u, 7u  //pines de control B de los motores (PWM)
};

static const bool VM_MOTOR_DIRECTION_INVERTED[VM_MOTOR_COUNT] = {
    false, false, false, false  //indica si la direccion del motor es invertida (true va hacia la izquierda, false va hacia la derecha)
};

#endif