#include "power.h"
#include "gpio.h"
#include "led.h"

#define MIN_TIMEOUT       SECONDS_TO_TICKS(60)

#ifdef DISABLE_SLEEP
#define VOLTAGE_LOW_ALARM 0 
#else
#define VOLTAGE_LOW_ALARM 3300 
#endif //DISABLE_SLEEP

static PowerMode power_mode = SNOOZE;
static uint32_t idle_timeout = SECONDS_TO_TICKS(1200);
static uint32_t idle_timer = SECONDS_TO_TICKS(1200);

void power_set_mode(PowerMode mode) {
  power_mode = mode;  
}

void power_set_timeout(uint32_t timeout) {
  idle_timeout = timeout;
  idle_timer = timeout;
}

void power_tick() {
  if (idle_timer-- == 0 && idle_timeout > MIN_TIMEOUT && power_mode != AWAKE) {
    power_mode = SLEEP;
  }
}

void power_idle_reset() {
  idle_timer = idle_timeout;
}

void power_management() {
  switch (power_mode) {
    case AWAKE:
      break;
    case SNOOZE:
      HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
      break;
    case SLEEP:
      HAL_DBGMCU_DisableDBGStandbyMode();
      HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
      __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
      HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
      __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
      HAL_PWR_EnterSTANDBYMode();
      break;
    default:
      break;
  }
}

