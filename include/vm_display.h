/**
 * @file vm_display.h
 * @brief LCD 20x4 I2C para la máquina expendedora (Hugo de León).
 *
 * Sustituye al "DisplaySink" del esquema UART: expone una función
 * `displayShow` que recibe 4 líneas y las presenta en la pantalla.
 */

#ifndef VM_DISPLAY_H
#define VM_DISPLAY_H

#include "vm_board_config.h"

class VmDisplay {
public:
    VmDisplay();
    void begin();
    void show(const char* line1, const char* line2,
              const char* line3, const char* line4);
};

#endif // VM_DISPLAY_H