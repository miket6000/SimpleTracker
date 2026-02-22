#include "task.h"
#include "tim.h"

static Task taskList[MAX_NUM_TASK];
static uint8_t numTask = 0;

Task *task_add(Task task) {
  uint32_t time = HAL_GetTick();
  if (numTask > (MAX_NUM_TASK - 1)) {
    return NULL;
  }
  task.lastRunTime = time - task.period + task.delay;
  taskList[numTask] = task;
  return &taskList[numTask++];
}

Task *task_build(uint32_t delay, uint32_t period, void (*callback)(void *), void *param) {
  uint32_t time = HAL_GetTick();
  if (numTask < MAX_NUM_TASK) {
    taskList[numTask].delay = delay;
    taskList[numTask].period = period;
    taskList[numTask].callback = callback;
    taskList[numTask].param = param;
    taskList[numTask].lastRunTime = time - period + delay;  
    return &taskList[numTask++];
  }
  while(1);
  return NULL;
}

void task_run() {
  uint32_t time = HAL_GetTick();
  for (uint8_t task = 0; task < numTask; task++) {
    if ((time - taskList[task].lastRunTime) >= taskList[task].period) {
      taskList[task].lastRunTime = time;
      taskList[task].callback(taskList[task].param);
    }
  }
}

