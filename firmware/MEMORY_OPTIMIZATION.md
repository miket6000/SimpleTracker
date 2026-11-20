# Memory Optimization: Lightweight Random Number Generator

## Problem
The firmware was exceeding the 32KB flash limit of the STM32F042G6 microcontroller. The inclusion of `rand()` from libc_nano was pulling in additional helper functions (malloc, printf infrastructure) that consumed ~200+ bytes of flash.

**Flash Usage:**
- STM32F042G6 has 32KB (31KB usable)
- Previous implementation: 33,236 bytes (.text section) - **OVERFLOW**
- Cause: `rand()` requires malloc/free infrastructure from libc

## Solution
Replaced the standard C `rand()` with a **lightweight Linear Congruential Generator (LCG)** that:
- Uses only ~20 bytes of code
- No external dependencies (no malloc, printf, etc.)
- Provides good pseudo-random distribution for slot allocation
- Saves ~200+ bytes of flash memory

## Implementation Details

### LCG Algorithm
```c
x = (1103515245 * x + 12345) & 0x7fffffff
```
- Parameters from POSIX.1c standard
- 32-bit state
- Fast single-line computation
- Good distribution for slot delays

### How It Works
1. **Initialization**: Call `lora_seed_prng(appContext.uid)` early in `main()`
   - Seeds with device UID to ensure each device gets unique sequence
   - Critical to avoid collisions when multiple devices use the same seed

2. **Random Slot Calculation**: Each LoRa broadcast response:
   - Calls `lora_calculate_random_delay()` 
   - LCG generates next pseudo-random number
   - Mapped to slot range: 500ms - 5000ms (9 slots of 500ms each)
   - Returns delay for task scheduler

3. **Task Scheduling**: Scheduled response waits the calculated delay before transmission

## Files Modified

### `Core/Inc/lora_arbitration.h`
- Added `lora_seed_prng()` function declaration
- Documented need for early initialization

### `Core/Src/lora_arbitration.c`
- Removed `#include <stdlib.h>` (no longer needed)
- Added `#include <stdint.h>` for uint32_t
- Implemented lightweight LCG PRNG:
  - `lora_prng_state`: 32-bit state variable
  - `lora_seed_prng()`: Initialize PRNG with seed
  - `lora_prng_next()`: Generate next pseudo-random number
  - `lora_calculate_random_delay()`: Use LCG for slot calculation

### `Core/Src/main.c`
- Added `#include "lora_arbitration.h"`
- Added `lora_seed_prng(appContext.uid)` call after UID initialization

## Testing

### Verify Memory Usage
Build and check the map file:
```bash
make clean
make
# Check build/firmware.map .text section size
# Should be significantly less than 31KB (previously 33KB)
```

### Verify Functionality
1. **Single Device**: Send broadcast UID request, verify response arrives with 500-5000ms delay
2. **Multiple Devices**: 
   - Configure 3+ devices with different UIDs
   - Send `T &ffffffffU` from ground station
   - Verify all responses received without collision
   - Check response spacing is multiples of 500ms

### Expected Delays
With proper seeding, devices should stagger:
- Device 1: May get 500ms, 1500ms, 2500ms, etc. depending on UID
- Device 2: Should get different values due to unique seed
- Device 3: Should get yet different sequence

## Verification Steps

1. **Before Build**:
   ```bash
   grep -n "lora_seed_prng" Core/Src/main.c
   # Should show the call near UID initialization
   ```

2. **After Build**:
   ```bash
   arm-none-eabi-nm build/firmware.elf | grep lora_
   # Should show: lora_calculate_random_delay, lora_seed_prng, lora_send_pending_response
   # Should NOT show: rand, srand, malloc, free
   ```

3. **Map File Check**:
   ```bash
   # In build/firmware.map, look for .text size
   # Previous: 0x81d4 (33,236 bytes)
   # New: Should be ~200 bytes less
   ```

## References

- **LCG Parameters**: POSIX.1c standard rand() implementation
- **Line 2160 of firmware.map**: Shows lib_a-rand.o was being linked
- **Memory Constraint**: STM32F042G6 = 32KB FLASH, 6KB RAM

## Future Notes

If you need cryptographic-grade randomness (unlikely for slot delays), you could:
- Use a hardware entropy source (if available on STM32)
- Implement AES-based PRNG
- But for this application, LCG is sufficient and much smaller

The LCG is now the only source of randomness in the firmware - if other parts need random numbers, they can call `lora_prng_next()` directly (just be aware it's not cryptographically secure).
