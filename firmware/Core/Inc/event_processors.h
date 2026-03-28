#ifndef EVENT_PROCESSORS_H 
#define EVENT_PROCESSORS_H

#include "app_context.h"
#include "event.h"
#include "gps.h"

void processGPSData(AppContext_t *context, char *nmeaSentence);
void processLoRaRx(AppContext_t *context, char *message, uint8_t len);
void processLoRaTx(AppContext_t *context, char *message);
void processLoRaGpsTx(AppContext_t *context, GpsPacket_t *packet);
void processLoRaTxDone(AppContext_t *context);

#endif // EVENT_PROCESSORS_H
