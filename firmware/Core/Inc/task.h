#ifndef TASK_H
#define TASK_H
#include <stdint.h>

#define MAX_NUM_TASK  5

typedef struct {
  void (*callback)(void *);
  void *param;
  uint32_t delay;
  uint32_t period;
  uint32_t lastRunTime;
} Task;

Task *task_add(Task task);
Task *task_build(uint32_t delay, uint32_t period, void (*callback)(void *), void *param);
void task_run();

#endif //TASK_H
