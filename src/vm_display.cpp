// Implementacion de pantalla LCD 20x4 via I2C

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "vm_display.h"

// Instancia global del controlador de pantalla
static LiquidCrystal_I2C s_lcd(VM_LCD_ADDR, VM_LCD_COLS, VM_DISPLAY_LINE_COUNT);

VmDisplay::VmDisplay() {
}

void VmDisplay::begin() {
    s_lcd.init();
    s_lcd.backlight();
}

void VmDisplay::show(const char* line1, const char* line2, const char* line3, const char* line4) {
    const char* lines[VM_DISPLAY_LINE_COUNT] = { line1, line2, line3, line4 };

    s_lcd.clear();

    for (uint8_t row = 0; row < VM_DISPLAY_LINE_COUNT; row++) {
        s_lcd.setCursor(0, row);

        const char* src = lines[row];
        for (uint8_t col = 0; col < VM_LCD_COLS; col++) {
            char c = ' ';
            if (src != NULL) {
                c = src[col];
                if (c == '\0') {
                    src = NULL;   // Rellenar con espacios al terminar la cadena
                    c = ' ';
                } else if ((c < 0x20) || (c > 0x7E)) {
                    c = ' ';      // Limpiar caracteres no imprimibles
                }
            }
            s_lcd.write((uint8_t)c);
        }
    }
}