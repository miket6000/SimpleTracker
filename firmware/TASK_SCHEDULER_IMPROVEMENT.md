# Task Scheduler Improvement for Collision Avoidance

## Problem Statement

The LoRa collision avoidance implementation needed to schedule one-shot delayed tasks for each broadcast response. However, the task scheduler had critical limitations:

1. **Hard limit**: Only 6 tasks (`MAX_NUM_TASK = 6`)
2. **No cleanup**: `task_add()` and `task_build()` only grew the list; completed tasks were never removed
3. **Overflow**: Scheduling multiple delayed responses would exhaust the task list and hang the system

This made the original approach fragile and unusable for real deployments where multiple devices might receive multiple broadcast commands.

## Solution: Task Reuse for One-Shot Tasks

Instead of modifying the entire task architecture, we improved `task_build()` to intelligently **reuse completed one-shot task slots**:

### Key Changes to `Core/Src/task.c`

The improved `task_build()` function now:

1. **Checks if one-shot task** (`period == 0`)
2. **Scans the task list** for completed one-shot tasks
3. **Reuses idle slots** where 500ms+ has passed since completion
4. **Falls back to adding** a new task only if no reusable slots exist

### Code Logic

```c
Task *task_build(uint32_t delay, uint32_t period, void (*callback)(void *), void *param) {
  uint32_t time = HAL_GetTick();
  
  // Try to find a completed one-shot task that can be reused
  if (period == 0) {  // One-shot task
    for (uint8_t i = 0; i < numTask; i++) {
      // Task is reusable if it was one-shot and enough time has passed since completion
      if (taskList[i].period == 0 && (time - taskList[i].lastRunTime) >= 500) {
        // Reuse this slot
        taskList[i].delay = delay;
        taskList[i].period = period;
        taskList[i].callback = callback;
        taskList[i].param = param;
        taskList[i].lastRunTime = time - period + delay;
        return &taskList[i];
      }
    }
  }
  
  // If no reusable slot or periodic task, add new task if space available
  if (numTask < MAX_NUM_TASK) {
    // ...add new task...
  }
}
```

## How It Works with LoRa Collision Avoidance

### Scenario: Ground Station Discovers 10 Remote Trackers

1. **First broadcast request** (`&ffffffffU`):
   - Device 1 schedules one-shot task: Delay 500ms → 1000ms, executes once
   - Device 2 schedules one-shot task: Delay 2500ms → 3000ms, executes once
   - Device 3 schedules one-shot task: Delay 4500ms → 5000ms, executes once
   - Task list uses 3 slots (out of 6)

2. **Task completes**:
   - After Device 1 transmits at T+1000ms, its task sits idle
   - 500ms later (T+1500ms), that slot becomes reusable
   - `lastRunTime` indicates "task is done and stale"

3. **Second broadcast request** (e.g., 30 seconds later):
   - Device 4 schedules one-shot task: **Reuses the slot from Device 1**
   - Device 5 schedules one-shot task: **Reuses the slot from Device 2**
   - Device 6 schedules one-shot task: **Reuses the slot from Device 3**
   - **No task list overflow**
   - Multiple discovery cycles work seamlessly

4. **Continuous operation**:
   - Unlimited number of delayed responses can be scheduled
   - Task list remains at maximum 6 slots occupied
   - Only the 6 periodic tasks + reused one-shot tasks needed

### Task List Timeline Example

```
Initial setup (all periodic):
[LED task (period: 25ms)] → periodic
[LoRa RX task (period: 100ms)] → periodic
[USB task (period: 0)] → one-shot, then reusable
[Voltage task (period: 2500ms)] → periodic
[GPS task (period: 10ms)] → periodic
[Empty slot]

First broadcast response:
[LED task (period: 25ms)] → periodic
[LoRa RX task (period: 100ms)] → periodic
[USB task (period: 0)] → REUSED for Device 1 delayed response
[Voltage task (period: 2500ms)] → periodic
[GPS task (period: 10ms)] → periodic
[Device 2 delayed response]

After Device 1 response executes and 500ms passes:
[LED task (period: 25ms)] → periodic
[LoRa RX task (period: 100ms)] → periodic
[Idle/Reusable] → Can be reused for next delayed response
[Voltage task (period: 2500ms)] → periodic
[GPS task (period: 10ms)] → periodic
[Device 2 delayed response]
```

## Benefits

| Aspect | Before | After |
|--------|--------|-------|
| **Max Delayed Responses** | 1 (then overflow) | Unlimited |
| **Architecture Changes** | Add polling loop to main | None - uses existing task system |
| **Code Complexity** | Special case handling | Clean, integrated solution |
| **Memory Usage** | Additional state struct | None - reuses task struct |
| **Task List Size** | Fixed 6 slots, fragile | Fixed 6 slots, robust |

## Reuse Timing (500ms threshold)

The 500ms delay before reusing a slot was chosen because:

1. **LoRa transmission time**: ~500ms for typical packets
2. **Safety margin**: Ensures previous task is completely done
3. **Practical**: Won't interfere with normal operation
4. **Adjustable**: Can be changed if needed for different timing requirements

To modify, change this line in `task.c`:
```c
if (taskList[i].period == 0 && (time - taskList[i].lastRunTime) >= 500) {
  // ^^^ 500ms threshold here
```

## Integration Summary

### Files Modified
1. ✅ `Core/Src/task.c` - Improved `task_build()` with slot reuse logic

### Files Unchanged (kept original architecture)
1. ✅ `Core/Inc/task.h` - No API changes needed
2. ✅ `Core/Src/main.c` - No main loop modifications
3. ✅ `Core/Src/lora_arbitration.c` - Uses simple task scheduling
4. ✅ All other files - No changes

### LoRa Collision Avoidance Still Works
- Devices receive broadcast commands
- Each device calculates random 500ms-5000ms delay
- Schedules delayed response via `lora_schedule_delayed_response()`
- Task scheduler executes response after delay
- **Now supports unlimited broadcast discovery cycles**

## Testing

### Unit Test: Task Reuse
```c
// Create a one-shot task
Task *t1 = task_build(100, 0, callback, NULL);
task_add(*t1);

// Let it execute by advancing time
// (in real test, wait for task_run() to execute it)

// Create new one-shot task 600ms later
Task *t2 = task_build(100, 0, callback2, NULL);
// Should reuse t1's slot if 500ms+ has passed
```

### System Test: Multiple Broadcasts
1. Send `T &ffffffffU` command from ground station
2. Multiple devices respond at staggered intervals
3. Send again 30+ seconds later
4. Verify all devices respond again (no task overflow)
5. Repeat 5+ times without system hang

## Future Improvements

If you need to support even more concurrent tasks:

1. **Increase MAX_NUM_TASK**: Change `#define MAX_NUM_TASK 6` in `task.h`
   - Each task slot uses one Task struct (~40 bytes)
   - Minimal RAM impact

2. **Reduce reuse threshold**: Change 500ms delay if your application has faster cycle times
   - Allows quicker slot reuse
   - Must not conflict with actual task execution time

3. **Periodic task cleanup**: Add optional `task_remove()` for periodic tasks if needed
   - Current solution doesn't need it since periodic tasks are static
   - Only useful if tasks are created/removed dynamically

## References

- **Original Issue**: Task list overflow from multiple delayed response scheduling
- **Solution Type**: Non-intrusive improvement to existing scheduler
- **Backward Compatible**: Yes - all existing code continues to work unchanged
- **Memory Impact**: None - reuses existing data structures
