#include "task.h"
#include "tim.h"

static Task taskList[MAX_NUM_TASK];
static uint8_t nextTask = 0;

Task *task_add(Task task) {
  uint32_t time = HAL_GetTick();
  if (nextTask > (MAX_NUM_TASK - 1)) {
    return NULL;
  }
  task.lastRunTime = time - task.period + task.delay;
  taskList[nextTask] = task;
  return &taskList[nextTask++];
}

Task *task_build(uint32_t delay, uint32_t period, void (*callback)(void *), void *param) {
  uint32_t time = HAL_GetTick();
  taskList[nextTask].delay = delay;
  taskList[nextTask].period = period;
  taskList[nextTask].callback = callback;
  taskList[nextTask].param = param;
  taskList[nextTask].lastRunTime = time - period + delay;  
  return &taskList[nextTask++];
}

void task_run() {
  uint32_t time = HAL_GetTick();
  for (uint8_t task = 0; task < nextTask; task++) {
    if ((time - taskList[task].lastRunTime) >= taskList[task].period) {
      taskList[task].lastRunTime = time;
      taskList[task].callback(taskList[task].param);
    }
  }
}

