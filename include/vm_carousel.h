/**
 * @file vm_carousel.h
 * @brief Carrousel de subpantallas del LCD 20x4 (portado del ESP32).
 *
 * Presenta `buildScreen` una vez por intervalo en el LCD vía `DisplayFn`.
 */

#ifndef VM_CAROUSEL_H
#define VM_CAROUSEL_H

#include <Arduino.h>
#include "vm_board_config.h"

#define LCD_LINE_LEN          VM_DISPLAY_LINE_LEN
#define LCD_LINE_COUNT        VM_DISPLAY_LINE_COUNT
#define CAROUSEL_MAX_SLIDES   6u

typedef void (*VmCarouselCallback)(const char* line1, const char* line2,
                                   const char* line3, const char* line4);

class VmCarousel {
public:
    VmCarousel(VmCarouselCallback callback);
    void begin();
    void update();
    void addScreen(void (*buildFn)(uint8_t index, char lines[LCD_LINE_COUNT][LCD_LINE_LEN]));
    void clearScreens();
    uint8_t currentIndex() const { return _currentIndex; }

private:
    VmCarouselCallback _callback;
    void (*_screens[CAROUSEL_MAX_SLIDES])(uint8_t index,
                                          char lines[LCD_LINE_COUNT][LCD_LINE_LEN]);
    uint8_t _numScreens;
    uint8_t _currentIndex;
    unsigned long _lastSwitchMs;
};

#endif // VM_CAROUSEL_H