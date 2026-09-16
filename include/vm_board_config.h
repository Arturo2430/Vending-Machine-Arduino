/**
 * @file vm_board_config.h
 * @brief Parámetros de capa física del Arduino Mega correspondientes a la
 *        responsabilidad de Eder Omar Zúñiga Zavala (UART v2, puerta,
 *        barrera y registro EEPROM).
 *
 * Fuera del alcance del contrato UART de Arquitectura (sección 1.3 del
 * contrato): baudrate, pines GPIO y niveles los definen Electrónica.
 *
 * Este archivo es específico del proyecto Arduino Mega. El proyecto del
 * ESP32 tiene su propio vm_board_config.h con los pines de su UART2, pero
 * AMBOS DEBEN compartir el mismo VM_UART_BAUDRATE (38400), ya que los dos
 * extremos deben coincidir en velocidad para entenderse.
 *
 * Mapa de pines según "Mapa de pines del Arduino Mega" (PDF 1).
 */

#ifndef VM_BOARD_CONFIG_H
#define VM_BOARD_CONFIG_H

/* ============================================================
 * UART hacia el ESP32
 * ============================================================
 * En el Mega, Serial1 es de hardware: TX1 = D18, RX1 = D19, con
 * adaptación de niveles 5V <-> 3.3V por parte de Electrónica.
 */
#define VM_UART_BAUDRATE         38400u

/* ============================================================
 * Sensores de seguridad (puerta y barrera - responsabilidad de Eder)
 * ============================================================ */
#define VM_PIN_BARRIER           2u   // Barrera óptica de caída (entrada)
#define VM_PIN_DOOR              3u   // Sensor de puerta cerrada (entrada)

// Nivel lógico activo de cada sensor (lo define Electrónica).
// Ajustar aquí si el sensor es activo-alto en lugar de activo-bajo.
#define VM_BARRIER_OCCUPIED_LEVEL LOW
#define VM_DOOR_CLOSED_LEVEL      LOW
#define VM_DOOR_DEBOUNCE_MS       30u   // Estabilidad exigida a la puerta

/* ============================================================
 * EEPROM (registro del último resultado físico - responsabilidad de Eder)
 * ============================================================ */
#define VM_EEPROM_START_ADDR     0u

#endif // VM_BOARD_CONFIG_H