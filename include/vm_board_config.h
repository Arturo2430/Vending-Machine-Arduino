/**
 * @file vm_board_config.h
 * @brief Parámetros de hardware del Arduino Mega 2560.
 *
 * La máquina opera de forma autónoma en un único Arduino Mega:
 * teclado 4x4, LCD 20x4 I2C, motores DC vía PCA9685,
 * lector RFID MFRC522 (SPI), sensor de puerta (D3),
 * barrera óptica de caída (D2) y persistencia EEPROM.
 */

#ifndef VM_BOARD_CONFIG_H
#define VM_BOARD_CONFIG_H

/* ============================================================
 * Sensores de seguridad (puerta y barrera óptica)
 * ============================================================ */
#define VM_PIN_BARRIER           2u   // Barrera óptica de caída (entrada)
#define VM_PIN_DOOR              3u   // Sensor de puerta cerrada (entrada)

#define VM_BARRIER_OCCUPIED_LEVEL LOW  // Nivel activo cuando hay producto
#define VM_DOOR_CLOSED_LEVEL      LOW  // Nivel activo cuando la puerta está cerrada
#define VM_DOOR_DEBOUNCE_MS       30u  // Filtro anti-rebote del sensor de puerta

/* ============================================================
 * Teclado 4x4 (filas D22-D25, columnas D26-D29)
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
 * EEPROM
 * ============================================================ */
#define VM_EEPROM_START_ADDR     0u

/* ============================================================
 * Timeouts de la FSM (milisegundos)
 * ============================================================ */
#define VM_TIMEOUT_INACTIVITY_MS    180000UL  // 180 s de inactividad → reposo
#define VM_TIMEOUT_MOTOR_MS          10000UL  // 10 s máx. de giro de motor
#define VM_CAROUSEL_INTERVAL_MS       2000UL  // 2 s por subpantalla del carrusel

/* ============================================================
 * Datos semilla de primer arranque
 * ============================================================ */
#define VM_SEED_CASH_DEFAULT_QTY   10u  // Monedas de cada tipo en caja al inicio

/* ============================================================
 * RFID MFRC522 (SPI del Arduino Mega)
 *   SS/SDA → D53   RST → D8
 *   SCK    → D52   MISO → D50   MOSI → D51
 *   VCC    → 3.3 V   GND → GND
 * ============================================================ */
#define VM_PIN_RFID_SS            53u
#define VM_PIN_RFID_RST            8u
#define VM_RFID_COOLDOWN_MS     1000u  // Pausa mínima entre lecturas RFID

#endif // VM_BOARD_CONFIG_H
