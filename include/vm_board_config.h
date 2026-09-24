/**
 * @file vm_board_config.h
 * @brief Parámetros de capa física del Arduino Mega (versión 2.1.0
 *        monoprocesador: sin ESP32, sin web, sin WiFi, sin RFID).
 *
 * La máquina opera de forma autónoma en el Mega: teclado, LCD 20x4 I2C,
 * motores DC vía PCA9685, sensores de puerta (D3) y barrera (D2), y
 * persistencia EEPROM.
 */

#ifndef VM_BOARD_CONFIG_H
#define VM_BOARD_CONFIG_H

/* ============================================================
 * Sensores de seguridad (puerta y barrera)
 * ============================================================ */
#define VM_PIN_BARRIER           2u   // Barrera óptica de caída (entrada)
#define VM_PIN_DOOR              3u   // Sensor de puerta cerrada (entrada)

// Nivel lógico activo de cada sensor (lo define Electrónica).
#define VM_BARRIER_OCCUPIED_LEVEL LOW
#define VM_DOOR_CLOSED_LEVEL      LOW
#define VM_DOOR_DEBOUNCE_MS       30u

/* ============================================================
 * Teclado 4x4 (v2.1.0: filas D22-D25, columnas D26-D29)
 * ============================================================ */
#define VM_KEYPAD_ROWS           4u
#define VM_KEYPAD_COLS           4u
#define VM_KEYPAD_DEBOUNCE_MS    25u

#define VM_KEYPAD_ROW_0          22u
#define VM_KEYPAD_ROW_1          23u
#define VM_KEYPAD_ROW_2          24u
#define VM_KEYPAD_ROW_3          25u
#define VM_KEYPAD_COL_0          26u
#define VM_KEYPAD_COL_1          27u
#define VM_KEYPAD_COL_2          28u
#define VM_KEYPAD_COL_3          29u

/* ============================================================
 * LCD I2C 20x4 (dirección 0x27)
 * ============================================================ */
#define VM_LCD_ADDR             0x27u
#define VM_LCD_COLS              20u
#define VM_LCD_ROWS               4u
#define VM_DISPLAY_LINE_LEN      VM_LCD_COLS
#define VM_DISPLAY_LINE_COUNT    VM_LCD_ROWS

/* ============================================================
 * EEPROM (persistencia mínima + última orden para recuperación Q20)
 * ============================================================ */
#define VM_EEPROM_START_ADDR     0u

/* ============================================================
 * Timeouts de la FSM (milisegundos)
 * ============================================================ */
#define VM_TIMEOUT_INACTIVITY_MS    180000UL  // 180 s en modos de compra/admin
#define VM_TIMEOUT_MOTOR_MS          10000UL  // 10 s sin RESULT del motor
#define VM_CAROUSEL_INTERVAL_MS       2000UL  // 2 s por subpantalla
#define VM_PIN_LOCKOUT_MS            30000UL  // 30 s tras 3 PIN fallidos

/* ============================================================
 * Datos semilla de primer arranque
 * ============================================================ */
#define VM_SEED_ADMIN_PIN          "1234"
#define VM_SEED_CASH_DEFAULT_QTY   10u

#endif // VM_BOARD_CONFIG_H
