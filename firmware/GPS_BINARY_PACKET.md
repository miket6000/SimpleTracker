# GPS Binary Packet Design

## Motivation

The tracker currently transmits the raw GNGGA NMEA sentence over LoRa. A typical sentence is ~70 bytes:

```
$GNGGA,123519.00,4807.03800,N,01131.00000,E,1,08,0.9,545.4,M,47.0,M,,*47
```

With the LoRa message format prepending 9 bytes (`<uid> `), the total on-air payload is ~80 bytes. At SF9/62.5 kHz this results in a long time-on-air, increasing collision probability, power consumption, and regulatory duty-cycle usage.

By packing only the unique, changing data into a binary structure we can dramatically reduce packet size.

---

## GNGGA Field Analysis

| # | Field          | Example        | Bytes (ASCII) | Changes? | Notes |
|---|----------------|----------------|---------------|----------|-------|
| 0 | Header         | `$GNGGA`       | 6             | Never    | Implied by packet type — omit |
| 1 | UTC Time       | `123519.00`    | 9             | Always   | hhmmss.ss — fractional seconds often `.00` |
| 2 | Latitude       | `4807.03800`   | 10            | Always   | ddmm.mmmmm |
| 3 | N/S Indicator  | `N`            | 1             | Rarely   | Only two values — 1 bit |
| 4 | Longitude      | `01131.00000`  | 11            | Always   | dddmm.mmmmm |
| 5 | E/W Indicator  | `E`            | 1             | Rarely   | Only two values — 1 bit |
| 6 | Fix Quality    | `1`            | 1             | Sometimes| 0–6 range — 3 bits sufficient |
| 7 | Satellites     | `08`           | 2             | Sometimes| 0–99 range, practically 0–24 |
| 8 | HDOP           | `0.9`          | 1–4           | Sometimes| 0.0–99.9 |
| 9 | MSL Altitude   | `545.4`        | 1–7           | Always   | Metres, can be negative |
|10 | Alt Unit       | `M`            | 1             | Never    | Always 'M' — omit |
|11 | Geoidal Sep.   | `47.0`         | 1–6           | Rarely   | Metres, can be negative |
|12 | Sep Unit       | `M`            | 1             | Never    | Always 'M' — omit |
|13 | Diff Corr Age  | (empty)        | 0             | Rarely   | Usually empty, only with DGPS — omit |
|14 | Diff Station ID| (empty)        | 0             | Rarely   | Usually empty, only with DGPS — omit |
|15 | Checksum       | `*47`          | 3             | Always   | Redundant in a binary packet — omit |

**Fields to omit entirely:** Header (0), Alt Unit (10), Sep Unit (12), Diff Corr Age (13), Diff Station ID (14), Checksum (15).

**Fields to pack:** Time (1), Latitude (2), N/S (3), Longitude (4), E/W (5), Fix Quality (6), Satellites (7), HDOP (8), MSL Altitude (9), Geoidal Separation (11).

---

## Encoding Decisions

### Time (Field 1)
NMEA format: `hhmmss.ss` — the fractional seconds from consumer GPS modules are typically `.00` and don't provide useful precision for a rocket tracker.

**Encoding:** Whole seconds since midnight = `hh*3600 + mm*60 + ss`. Max value = 86399, fits in 17 bits. Stored as a `uint32_t` for alignment simplicity (no bit-packing libraries needed on a Cortex-M0).

**Size: 4 bytes** (as `uint32_t`, upper bits available for future use)

### Latitude (Fields 2+3)
NMEA format: `ddmm.mmmmm` with 5 decimal places in the fractional minutes. The value `4807.03800` means 48° 07.03800'.

To avoid floating-point maths on the Cortex-M0 (no FPU), we store the value as an integer in units of 1/100000th of a minute. The integer range is 0–9000.00000 → 0–900000000, which fits in 30 bits. Negative latitudes (S hemisphere) could use bit 31 as a sign bit.

**Encoding:** Parse the NMEA string as a fixed-point integer (strip the decimal point, treat as integer). Store as a signed `int32_t` where negative indicates South.

- Example: `4807.03800,N` → `+480703800`
- Example: `3342.12345,S` → `-334212345`

Max magnitude: 9000.00000 → 900000000 — fits in 30 bits signed.

**Size: 4 bytes** (as `int32_t`)

### Longitude (Fields 4+5)
Same as latitude but NMEA format is `dddmm.mmmmm`, range 0–18000.00000 → 0–1800000000, which fits in 31 bits.

**Encoding:** Same approach. Signed `int32_t`, negative for West.

- Example: `01131.00000,E` → `+113100000`
- Example: `12259.99999,W` → `-1225999999`

Max magnitude: 1800000000 — fits in 31 bits signed.

**Size: 4 bytes** (as `int32_t`)

### Fix Quality (Field 6)
Range 0–6. Fits easily in the flags byte (3 bits).

### N/S and E/W Indicators (Fields 3, 5)
Absorbed into the sign of the latitude/longitude integers (see above). No separate storage needed.

### Satellites (Field 7)
Range 0–99 (practically 0–24 for GPS + GLONASS). A `uint8_t` is sufficient.

**Size: 1 byte**

### HDOP (Field 8)
Range typically 0.0–99.9. Stored as `uint8_t` with a scale factor of ×10 (i.e. 0.9 → 9, 25.5 → 255). Max representable 25.5, which covers all "useful" HDOP values (anything above ~20 is very poor fix). Values above 25.5 can be clamped to 255.

**Size: 1 byte**

### MSL Altitude (Field 9)
Range: -1000.0 to ~100000.0 metres for rocketry use. Storing in decimetres (×10) as a signed `int32_t` gives 0.1 m resolution. Max representable ±214,748,364.7 m — vastly more than needed.

However, for a rocket tracker, we can use `int16_t` in whole metres: range -32768 to +32767 m (−32 km to +32 km). This covers hobby rocketry altitudes with ample margin and saves 2 bytes. Resolution of 1 metre is acceptable.

**Encoding:** Altitude in whole metres as `int16_t`.

**Size: 2 bytes**

### Geoidal Separation (Field 11)
Range roughly -150 to +150 metres globally. Changes very slowly (essentially per-region). Stored as `int16_t` in decimetres (×10) for 0.1 m resolution. Range: ±3276.7 m — more than sufficient.

**Size: 2 bytes**

### Flags Byte
Combine fix quality (3 bits) with reserved bits for future use:

```
Bit 7..3: Reserved (set to 0)
Bit 2..0: Fix quality (0–6)
```

**Size: 1 byte**

---

## Final Structure

```c
// Packed GPS binary packet for LoRa transmission
// Total: 19 bytes
typedef struct __attribute__((packed)) {
    uint32_t  time_s;       // Seconds since midnight UTC (0–86399)
    int32_t   latitude;     // ddmm.mmmmm as integer ×100000, negative = South
    int32_t   longitude;    // dddmm.mmmmm as integer ×100000, negative = West
    int16_t   altitude;     // MSL altitude in whole metres
    int16_t   geoid_sep;    // Geoidal separation in decimetres (÷10 for metres)
    uint8_t   satellites;   // Number of satellites in use
    uint8_t   hdop;         // HDOP × 10 (0.1 resolution, max 25.5)
    uint8_t   flags;        // Bit 2..0: fix quality (0–6), Bit 7..3: reserved
} GpsPacket_t;
```

**Total: 19 bytes** (with `__attribute__((packed))`)

### Comparison

| Format | Payload Size | With UID prefix | Saving |
|--------|-------------|-----------------|--------|
| Raw NMEA | ~70 bytes | ~80 bytes | — |
| Binary packet | 19 bytes | 28 bytes | **65% reduction** |

---

## Parsing Strategy (Firmware — No External Libraries)

All parsing is done with simple integer arithmetic on the existing NMEA string. No `atof()`, `sscanf()`, or floating-point operations are needed.

### Latitude/Longitude Integer Parsing

The NMEA field `4807.03800` can be parsed as follows:
1. Walk the string character by character.
2. Accumulate digits into an integer, skipping the decimal point.
3. The result for `4807.03800` is `480703800`.

This is a straightforward integer parse that handles the decimal point by simply ignoring it. Since the NMEA spec guarantees 5 decimal places for GNGGA, the result is always in units of 1/100000th of a minute.

If the corresponding N/S or E/W indicator is `'S'` or `'W'`, negate the result.

### Altitude and Geoidal Separation

These are decimal numbers like `545.4` or `-17.2`. Parse similarly:
1. Track the sign.
2. Accumulate the integer part.
3. For altitude (whole metres): discard the fractional part after the decimal point.
4. For geoidal separation (decimetres): keep one fractional digit.

### HDOP

Parse the integer and first fractional digit to produce a ×10 value. E.g. `0.9` → `9`, `2.5` → `25`.

### Time

Parse `hhmmss` portion: `hh*3600 + mm*60 + ss`. Ignore fractional seconds.

---

## Decoding at the Ground Station

The ground station receives the 19-byte binary packet and can reconstruct readable values:

```
latitude_deg  = packet.latitude / 100000 / 100  (integer division for degrees)
latitude_min  = (packet.latitude / 100000) % 100 + (packet.latitude % 100000) / 100000.0
altitude_m    = packet.altitude
geoid_sep_m   = packet.geoid_sep / 10.0
hdop          = packet.hdop / 10.0
```

Or the ground station can simply forward the raw binary to the PC application via USB (perhaps hex-encoded or base64), where the PC application performs the decoding. This avoids any floating-point reconstruction on the microcontroller.

---

## Impact on Existing Code

### Sender side (`event_gps.c` / `event_lora_tx.c`)
- `event_gps.c` will parse the NMEA sentence into a `GpsPacket_t` instead of just saving a string pointer.
- `event_lora_tx.c` will transmit the binary packet bytes instead of the ASCII string.

### Receiver side (`event_lora_rx.c`)
- The non-command message path (GPS data) will receive binary bytes instead of an NMEA string.
- The ground station can either decode back to NMEA for USB output (for backward compatibility with the PC app) or relay the binary data in a new format.

### Header changes
- `GpsPacket_t` struct definition should go in `gps.h`.
- A packing function (e.g. `gps_pack_sentence()`) should be added to `gps.c` or a new utility file.

---

## Design Rationale Summary

| Decision | Rationale |
|----------|-----------|
| `__attribute__((packed))` | STM32F042 is Cortex-M0, supports unaligned access via byte-lane routing. Packed struct avoids padding bytes in the LoRa payload. GCC on ARM supports this efficiently. |
| Integer-only parsing | No FPU on Cortex-M0. Avoids pulling in soft-float library code, saving flash. |
| Signed lat/lon absorbs N/S, E/W | Eliminates 2 bytes and simplifies decoding — standard convention. |
| HDOP as ×10 `uint8_t` | 0.1 resolution sufficient; max 25.5 covers all practical values; saves vs `int16_t`. |
| Altitude as whole metres `int16_t` | ±32 km range is generous for rocketry. 1 m resolution is adequate. Saves 2 bytes vs `int32_t`. |
| Geoidal separation in decimetres `int16_t` | 0.1 m resolution is adequate; the value barely changes during a flight. |
| `uint32_t` for time | Natural alignment, simple arithmetic, and upper bits available for flags/versioning if needed in the future. |
| No bit-packing across byte boundaries | Keeps code simple, avoids shift/mask complexity, and the 19-byte result is already a huge improvement over ~70 bytes. |
| No external libraries | Critical constraint — flash is nearly full at 32 KB. All parsing uses basic C string/integer operations already available. |
