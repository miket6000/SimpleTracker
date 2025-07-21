#ifndef EVENT_PROCESSORS_H 
#define EVENT_PROCESSORS_H

#include "app_context.h"
#include "event.h"

void processGPSData(AppContext_t *context, char *nmeaSentence);
void processLoRaRx(AppContext_t *context, char *message);
void processLoRaTx(AppContext_t *context, char *message);

#endif // EVENT_PROCESSORS_H
