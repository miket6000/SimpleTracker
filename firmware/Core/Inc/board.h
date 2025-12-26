#ifndef BOARD_H
#define BOARD_H

#include "led.h"

typedef struct {
  LedHandle *blue;
  LedHandle *green;
} Leds;

#endif // BOARD_H
