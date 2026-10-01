#ifndef VM_TYPES_H
#define VM_TYPES_H

#include <stdint.h>

// Resultado físico del dispensado
typedef enum {
    VM_RESULT_DELIVERED              = 0x00,
    VM_RESULT_REJECTED_BEFORE_MOTION = 0x01, // Barrera ocupada antes del giro
    VM_RESULT_UNCERTAIN              = 0x02  // Giro sin detección de caída
} vm_dispense_result_t;

#define VM_CHANNEL_MIN 1u
#define VM_CHANNEL_MAX 4u

#endif // VM_TYPES_H
