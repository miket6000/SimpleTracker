#ifndef LINKTEST_H
#define LINKTEST_H

#include "app_context.h"

#ifdef APP_LINKTEST
void linktest_boot(AppContext_t *context);
void linktest_tick(void);
#endif

#endif // LINKTEST_H
