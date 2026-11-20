# LoRa Collision Avoidance Implementation

## Overview
This implementation adds random slot delay arbitration to resolve LoRa message collisions when multiple remote trackers respond to the same broadcast command from a ground station.

## Problem Solved
Previously, when the ground station sent a broadcast command (e.g., `&ffffffffU` requesting UIDs), all devices in range would respond immediately, causing RF collisions where neither response reached the ground station successfully. This made it impossible to discover multiple devices.

## Solution: Random Slot Delay (MAC-style Arbitration)
Each device now:
1. Receives the broadcast command
2. Generates a random delay between `LORA_MIN_SLOT_DELAY_MS` and `LORA_MAX_SLOT_DELAY_MS`
3. Schedules the response to be sent after this delay
4. Transmits after the delay expires

With properly spaced delays (e.g., 500ms slots across a 0-5000ms window), the probability of collisions is greatly reduced, allowing the ground station to receive responses from multiple devices sequentially.

## Implementation Approach

Rather than creating a new task for each delayed response (which would overflow the 6-task limit), the implementation uses a **polling-based state machine**:

1. **Schedule Phase**: `lora_schedule_delayed_response()` stores the response data and calculates the target transmit time
2. **Poll Phase**: Main loop calls `lora_process_delayed_response()` which checks if it's time to send
3. **Send Phase**: When the time arrives, the response is transmitted and state is cleared

This allows unlimited delayed responses without overflowing the task list.

## Files Modified

### 1. `Core/Inc/app_context.h`
Added three fields to `AppContext_t` structure:
- `pendingResponseData`: Pointer to buffered response data
- `pendingResponseLength`: Length of the pending response
- `responseSlotDelay`: The calculated delay for this response

These track pending responses that are scheduled to be sent later.

### 2. New: `Core/Inc/lora_arbitration.h`
New header file containing the arbitration API:
- `lora_schedule_delayed_response()`: Queue a response with random delay
- `lora_send_pending_response()`: Send the queued response (called by task scheduler)
- `lora_calculate_random_delay()`: Generate random delay in milliseconds

Configuration constants (adjustable):
- `LORA_MIN_SLOT_DELAY_MS`: 0ms (minimum delay)
- `LORA_MAX_SLOT_DELAY_MS`: 5000ms (maximum delay)
- `LORA_SLOT_INTERVAL_MS`: 500ms (granularity of time slots)

### 3. New: `Core/Src/lora_arbitration.c`
Implementation of the arbitration mechanism:
- Uses a **lightweight Linear Congruential Generator (LCG)** instead of `rand()` to minimize code size
- Avoids linking in large portions of libc (saves ~200+ bytes vs. `rand()`)
- Static buffer for response data (256 bytes)
- Uses task scheduler to manage delayed transmission

**Important:** Call `lora_seed_prng(appContext.uid)` in `main()` early to seed the PRNG with device UID for uniqueness across devices

### 4. `Core/Src/event_lora_rx.c`
Modified `processLoRaRx()` to use delayed responses for:
- **'U' command** (Get UID): Now scheduled with delay instead of immediate transmission
- **'V' command** (Get voltage): Now scheduled with delay instead of immediate transmission
- **'T' command** (Transmit mode): Continues to send ACK immediately (not a response to a broadcast)
- **Other commands**: NAK still sent immediately (error response)

## How It Works

### Execution Flow
```
Ground Station sends: T &ffffffffU
                      ↓
Device receives EVENT_LORA_RX
                      ↓
processLoRaRx() checks if it's a broadcast command ('&' + 'ffffffff')
                      ↓
For 'U' command, calls lora_schedule_delayed_response()
                      ↓
Random delay calculated (e.g., 2137ms)
                      ↓
Task scheduler adds callback to send response after 2137ms
                      ↓
After 2137ms, lora_send_pending_response() is called
                      ↓
Response transmitted: uidStr (8 bytes, takes ~500ms on LoRa link)
```

### Scenario with Multiple Devices
With 3 devices (accounting for 500ms LoRa transmission time):
- Device A: Random delay = 500ms → transmission starts at T+500ms, ends ~T+1000ms
- Device B: Random delay = 2500ms → transmission starts at T+2500ms, ends ~T+3000ms
- Device C: Random delay = 4500ms → transmission starts at T+4500ms, ends ~T+5000ms

Ground station receives all three responses without collision!

## Integration Checklist

- [ ] Add `#include "lora_arbitration.h"` to Makefile if needed
- [ ] Ensure `rand()` is seeded in main.c (e.g., with UID for uniqueness)
- [ ] Test with multiple devices to verify collision avoidance
- [ ] Adjust `LORA_MIN_SLOT_DELAY_MS`, `LORA_MAX_SLOT_DELAY_MS` based on LoRa link speed
- [ ] Consider adding CRC/checksum to responses if not already present

## Customization

### Adjusting Timing
Edit `lora_arbitration.h`:
```c
#define LORA_MIN_SLOT_DELAY_MS   500   // Minimum delay (allows for 500ms LoRa transmission)
#define LORA_MAX_SLOT_DELAY_MS   5000  // Maximum delay (5 seconds for ~9 devices)
#define LORA_SLOT_INTERVAL_MS    500   // Each slot is 500ms (one LoRa packet duration)
```

**Guidelines:**
- `LORA_MIN_SLOT_DELAY_MS` should be ≥ maximum LoRa packet transmission time
- Each increment of `LORA_SLOT_INTERVAL_MS` allows one additional device to respond sequentially
- With 500ms slots and 5000ms max, you can support up to 10 devices responding without collision
- Smaller intervals = more devices but increased collision risk
- Larger intervals = fewer collisions but longer discovery time

### Random Seed
The implementation uses a lightweight Linear Congruential Generator (LCG) instead of `rand()` to save ~200+ bytes of flash memory. You must seed it early in `main()`:

```c
// In main.c, after appContext is initialized
lora_seed_prng(appContext.uid);  // Use device UID for seed variation
```

Without seeding, all devices will use the same PRNG sequence, which could cause collisions. Seeding with the UID ensures each device gets a unique sequence.

## Testing

### Single Device
- Send broadcast UID request
- Verify response arrives with a delay of 10-100ms

### Multiple Devices
- Configure multiple devices with different UIDs
- Send broadcast UID request from ground station
- Capture all responses to verify no collisions
- Responses should arrive at different times within the 10-100ms window

### Performance
- Measure time from request to complete discovery of N devices
- With 500ms slots and 5000ms window: ~10 devices in 5-5.5 seconds
- Device 1: 0.5s delay + 0.5s TX ≈ 1.0s total
- Device 10: 5.0s delay + 0.5s TX ≈ 5.5s total
- Adjust window size if you need to support more devices (e.g., 10s max = 20 devices)

## Future Enhancements

1. **Adaptive Delay**: Adjust `LORA_MIN/MAX_SLOT_DELAY_MS` based on number of detected devices
2. **Exponential Backoff**: Increase delay on collision detection (though RF collisions may be hard to detect)
3. **Device Registration**: Maintain a list of known devices to optimize discovery time
4. **Priority Responses**: Give certain devices (e.g., primary tracker) shorter delays
