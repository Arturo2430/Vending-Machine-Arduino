/**
 * @file vm_types.h
 * @brief Tipos y enumeraciones compartidos del firmware.
 */

#ifndef VM_TYPES_H
#define VM_TYPES_H

#include <stdint.h>

/* Resultado físico del dispensado. */
typedef enum {
    VM_RESULT_DELIVERED              = 0x00, // Producto cayó correctamente
    VM_RESULT_REJECTED_BEFORE_MOTION = 0x01, // Puerta abierta o barrera ocupada
    VM_RESULT_UNCERTAIN              = 0x02  // Motor giró pero sin confirmación
} vm_dispense_result_t;

/* Rango válido de canales (1 a 4). */
#define VM_CHANNEL_MIN 1u
#define VM_CHANNEL_MAX 4u

#endif // VM_TYPES_H
