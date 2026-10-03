// Controlador de pantalla LCD 20x4 via I2C

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