#ifndef VM_BOARD_CONFIG_H
#define VM_BOARD_CONFIG_H

// Barrera óptica de caída (D2, LOW = detección de producto)
#define VM_PIN_BARRIER             2u
#define VM_BARRIER_OCCUPIED_LEVEL  LOW

// Teclado 4x4
#define VM_KEYPAD_ROWS             4u
#define VM_KEYPAD_COLS             4u
#define VM_KEYPAD_DEBOUNCE_MS      25u

#define VM_KEYPAD_ROW_0            22u
#define VM_KEYPAD_ROW_1            23u
#define VM_KEYPAD_ROW_2            24u
#define VM_KEYPAD_ROW_3            25u
#define VM_KEYPAD_COL_0            30u
#define VM_KEYPAD_COL_1            31u
#define VM_KEYPAD_COL_2            33u
#define VM_KEYPAD_COL_3            29u

// LCD 20x4 I2C
#define VM_LCD_ADDR                0x27u
#define VM_LCD_COLS                20u
#define VM_LCD_ROWS                4u
#define VM_DISPLAY_LINE_LEN        (VM_LCD_COLS + 1u)
#define VM_DISPLAY_LINE_COUNT      VM_LCD_ROWS

// EEPROM
#define VM_EEPROM_START_ADDR       0u

// Timeouts FSM (ms)
#define VM_TIMEOUT_INACTIVITY_MS   180000UL
#define VM_TIMEOUT_MOTOR_MS        10000UL
#define VM_CAROUSEL_INTERVAL_MS    2000UL

// Semilla de caja
#define VM_SEED_CASH_DEFAULT_QTY   10u

// RFID MFRC522 (SPI Arduino Mega: SS=53, RST=8, SCK=52, MISO=50, MOSI=51)
#define VM_PIN_RFID_SS             53u
#define VM_PIN_RFID_RST            8u
#define VM_RFID_COOLDOWN_MS        1000u

#endif // VM_BOARD_CONFIG_H
