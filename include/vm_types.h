#ifndef VM_TYPES_H
#define VM_TYPES_H

#include <stdint.h>

// Resultado de operacion del motor
typedef enum {
    VM_RESULT_DELIVERED              = 0x00,
    VM_RESULT_REJECTED_BEFORE_MOTION = 0x01, // Obstruccion detectada
    VM_RESULT_UNCERTAIN              = 0x02  // Falla de sensor
} vm_dispense_result_t;

// Limites de canales disponibles
#define VM_CHANNEL_MIN 1u
#define VM_CHANNEL_MAX 4u

#endif // VM_TYPES_H
