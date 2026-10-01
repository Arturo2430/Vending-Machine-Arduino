#include <string.h>
#include "vm_carousel.h"

VmCarousel::VmCarousel(VmCarouselCallback callback)
    : _callback(callback),
      _numScreens(0),
      _currentIndex(0),
      _lastSwitchMs(0)
{
    memset(_screens, 0, sizeof(_screens));
}

void VmCarousel::begin() {
    _currentIndex = 0;
    // Forzar visualizacion inmediata en el primer update()
    _lastSwitchMs = (unsigned long)(millis() - VM_CAROUSEL_INTERVAL_MS);
}

void VmCarousel::addScreen(void (*buildFn)(uint8_t index,
                                           char lines[LCD_LINE_COUNT][LCD_LINE_LEN])) {
    if (_numScreens >= CAROUSEL_MAX_SLIDES) return;
    _screens[_numScreens] = buildFn;
    _numScreens++;
}

void VmCarousel::clearScreens() {
    _numScreens   = 0;
    _currentIndex = 0;
}

void VmCarousel::update() {
    if (_callback == NULL || _numScreens == 0) return;

    if ((uint32_t)(millis() - _lastSwitchMs) >= VM_CAROUSEL_INTERVAL_MS) {
        _lastSwitchMs = millis();

        uint8_t idx = _currentIndex % _numScreens;

        char lines[LCD_LINE_COUNT][LCD_LINE_LEN];
        memset(lines, 0, sizeof(lines));
        _screens[idx](idx, lines);
        _callback(lines[0], lines[1], lines[2], lines[3]);

        _currentIndex = (idx + 1u) % _numScreens;
    }
}