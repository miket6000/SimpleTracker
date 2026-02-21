#include <stdlib.h>
#include "app_context.h"
#include "event.h"
#include "adc.h"

void task_measure_voltage (void *param) {
    AppContext_t *context = (AppContext_t *)param;

    HAL_ADC_Start(&hadc);
    /* Wait for conversion to complete */
    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
    uint32_t value = (uint16_t)HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);
    
    // convert ADC to voltage. voltage = ADC / 2^12 * 2 * 3300mV
    value *= 6600;
    value >>= 12;
    context->voltage = (value & 0xFFFF);
}
