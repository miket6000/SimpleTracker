//#include "tasks.h"
#include "led.h"
#include "app_context.h"

void task_led(void *param) {
  AppContext_t *appContext = param;
  led_blink(appContext->gpsLed);
  led_blink(appContext->loraLed); 
}
