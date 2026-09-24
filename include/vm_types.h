/**
 * @file vm_types.h
 * @brief Tipos y enumeraciones compartidos del firmware (sin UART).
 *
 * Nota histórica: estos valores provenían del contrato UART SAID-ARCH-UART
 * v2.0.0. Como el proyecto ahora es de un solo Arduino, el enlace UART se
 * eliminó, pero se conservan los nombres/valores para no reescribir la
 * semántica de la FSM (modo, resultado físico, rango de canales).
 */

#ifndef VM_TYPES_H
#define VM_TYPES_H

#include <stdint.h>

/* Modo de operación (antes campo "mode" del contrato). */
typedef enum {
    VM_MODE_VENTA         = 0x00,
    VM_MODE_MANTENIMIENTO = 0x01
} vm_mode_t;

/* Resultado físico de dispensado (campo "result"). */
typedef enum {
    VM_RESULT_DELIVERED              = 0x00,
    VM_RESULT_REJECTED_BEFORE_MOTION = 0x01,
    VM_RESULT_UNCERTAIN              = 0x02
} vm_dispense_result_t;

/* Rango de canales de dispensado (VEND). */
#define VM_CHANNEL_MIN 1u
#define VM_CHANNEL_MAX 4u

#endif // VM_TYPES_H
