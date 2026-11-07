# SimpleTracker Communication Specification v2 (DRAFT)

Status: **Draft for review — not yet implemented.**

This specification replaces the v1 "Ground Station USB Communication Specification"
(and its implicit on-air message format) with a redesigned protocol covering both
planes:

| Plane | Link | Who talks on it |
|---|---|---|
| **USB plane** | USB CDC (virtual COM port), host ↔ Ground Station | PC/mobile app ↔ GS firmware |
| **Air plane** | LoRa, GS ↔ trackers | GS firmware ↔ tracker firmware |

The same firmware binary implements both roles; a device boots as a Ground
Station and only becomes a tracker after it is commanded over the air.

This is a **breaking redesign**. V1 protocol support is not a goal. An ASCII
debug channel is provided inside the binary protocol so field debugging does not
require a mode switch.

### Design constraints and goals

1. **Light on air.** Air messages are the primary bandwidth constraint
   (SF9 / 62.5 kHz, time-on-air of hundreds of ms per packet). Air payloads are
   binary and field minimal. Rough rule used throughout: *a byte saved on air is
   worth more than a debug feature.*
2. **No polling.** The app must not poll for state. All asynchronous device
   activity (discovery hits, pairing status, telemetry, voltage) is delivered as
   USB events, matched by an explicit *subscription* created by the host.
3. **Self-delimiting, length-prefixed USB framing** with a CRC — responses can
   exceed 64 bytes and no "flush partial line after 50 ms" hack is required.
4. **Fits the STM32F042** (32 KB flash / 6 KB RAM). Total new firmware
   implementation cost is budgeted at ≤ 3 KB flash and ≤ ~1 KB RAM, funded by the
   dead-code layoffs listed in the migration plan.
5. **Deterministic decode.** Telemetry is the existing 19-byte binary GPS packet
   passed through verbatim, with decode formulas in this document — no NMEA
   re-serialisation on the device.

---

## 1. Conventions

* All multi-byte fields are **little-endian** (matches Cortex-M0/SX1262 register order).
* `u8/u16/u32/i8/i32` = unsigned/signed integers of that width.
* Packed structs are **byte-aligned, no padding** (`__attribute__((packed))` / Java `ByteBuffer` with explicit order).
* `GpsPacket_t` is the 19-byte packed GPS structure already defined in
  `firmware/GPS_BINARY_PACKET.md`. Section 10 gives its decode rules; nothing in
  v2 re-serialises it to NMEA.

---

## 2. Transport (USB plane)

| Parameter | Value |
|---|---|
| Interface | USB CDC (TinyUSB, CDC-ACM) — no UART emulation semantics needed |
| Framing | Binary length-prefixed frames (§3) — no line endings |
| Encoding | Binary; ASCII allowed in `DBG` payload and in textual opcode fields |
| Max payload | 128 bytes (`LEN > 128` → frame dropped silently) |
| Baud rate | Ignored (USB CDC); host may open at any rate |
| DTR | Host must assert DTR (`tud_cdc_n_connected()` gate). Without DTR the device drops TX silently and still records events. |

Commands are strictly one-in-flight: the device does not pipeline (it returns
`RSP_ERR BUSY (4)` to a `CMD` arriving while it is executing one). This keeps the
firmware free of a command queue. Retry/timeout is the host's job (§5.4).

### 2.1 Receive path (device)

* Bytes are drained in the main loop; parsing state machine is run-to-completion,
  never called from interrupt or USB driver callback.
* Resync rule: scan forward for the sync pattern `A5 5A 02` (sync + `VER == 2`),
  then read `LEN`; if a claimed `LEN` would exceed the remaining plausible stream
  the parser resyncs. A frame with a bad CRC, bad `LEN`, or unknown `TYPE` is
  **dropped without response** and the device's drop counter is incremented (the
  device never ACKs malformed input — it cannot know the sequence number is sane).
* Back-to-back frames are legal; no inter-frame delay is required.

### 2.2 Transmit path (device)

* A TX ring buffer (≥ 256 bytes, i.e. two worst-case `EVT_TELEMETRY` frames)
  sized to outlive the 64-byte TinyUSB endpoint FIFO.
* `tud_cdc_write()` return values are **honoured**: partial writes re-arm the
  flush task and resume from the ring. This closes the v1 truncation defect
  (v1 §H2) by construction.
* If a frame queued in the ring would overflow, the oldest **event** frame is
  evicted (commands/responses are never evicted) and the eviction is signalled
  with `EVT_OVERFLOW`.

---

## 3. USB frame format

```
offset  size  field
0       2     SYNC  0xA5 0x5A
2       1     VER   0x02
3       1     TYPE
4       2     SEQ   u16 little-endian
6       2     LEN   u16 little-endian, payload bytes, ≤ 128
8       LEN   PAYLOAD
8+LEN   2     CRC16 u16 little-endian, over bytes [2 .. 8+LEN-1] (VER..payload end)
```

CRC16: **CCITT-FALSE** — poly `0x1021`, init `0xFFFF`, no reflection, no final
xor. Implemented with the 16-entry nibble table from Appendix B (32 B flash, ~80
ns/byte on M0 at 48 MHz).

### 3.1 Frame types

| TYPE | Name | Meaning | SEQ semantics |
|---|---|---|---|
| `0x01` | `CMD` | Host → device request | Host-assigned; monotonically increasing |
| `0x02` | `RSP_OK` | Terminal response to a `CMD`; may carry payload | Same SEQ as the `CMD` |
| `0x03` | `RSP_ERR` | Terminal error response to a `CMD` | Same SEQ as the `CMD`; payload = error code (+ optional ASCII detail) |
| `0x04` | `EVT` | Unsolicited device → host event | Device event counter (independent of `CMD` SEQ; wraps) |
| `0x05` | `DBG` | ASCII log/terminal text from device (opt-in via `DEBUG_MODE`) | Event counter |

Every `CMD` results in exactly one `RSP_OK` or `RSP_ERR`. Work that continues
after the response is reported via `EVT`s.

### 3.2 Error codes (`RSP_ERR` payload byte 0)

| Code | Meaning |
|---|---|
| 1 | `BAD_LEN` — payload length wrong for the opcode |
| 2 | `BAD_OPCODE` |
| 3 | `BAD_ARG` — argument value out of range / invalid UID |
| 4 | `BUSY` — command already executing |
| 5 | `NOT_SUPPORTED` — capability bit off (e.g. settings persistence) |
| 6 | `RADIO_BUSY` — radio mid-transmission/command-flooded |
| 7 | `NO_SUB` — unsubscribe of not-subscribed UID |
| 8 | `BLOCKED` — radio on a non-discovery channel where the command cannot run |

---

## 4. USB command catalogue

Payload of a `CMD` is `[opcode u8][args...]`. `RSP_OK` payload (if any) is stated
per command. A preceding `HELLO` is mandatory; the device treats any other
opcode as `RSP_ERR NOT_SUPPORTED` until `HELLO` is answered (this is also how a
host detects that it is talking to v2).

### 4.1 `HELLO` — 0x00

* Args: none.
* `RSP_OK`: `[fw_major u8][fw_minor u8][caps u16][uid u32]`
  * `caps` bits: 0 = `EVENTS`, 1 = `SUBSCRIBE`, 2 = `SETTINGS_PERSIST`,
    3 = `LOCAL_FIX_STREAM`. All bits except cap 2 are mandatory in v2.

### 4.2 `INFO` — 0x01

* Args: none.
* `RSP_OK`:

```
0  u32  freq_hz
4  u8   sf
5  u8   bw
6  u8   coding_rate
7  u8   tx_power_dbm
8  u8   mode      0 = ground station, 1 = tracker
9  u16  vbat_mv
11 u32  uptime_s
15 u8   fw_sel reserved = 0
```

### 4.3 `RADIO_CONFIG` — 0x03  (replaces v1 `C`)

* Args: `[freq u32][sf u8][bw u8][cr u8][tx_pwr u8]`
* `RSP_OK` immediately; radio re-enters receive on the new parameters before the
  response flushes.
* Valid ranges: `sf` 5–12, `bw` 0–6 (§9 enum), `cr` 1–4, `power` −9…22.
  * Subscriptions are unaffected (they are channel-agnostic; see §7.1).

### 4.4 `RADIO_READ` — 0x04

* Args: none. `RSP_OK`: same layout as `INFO` radio prefix (0..8).

### 4.5 `DISCOVER` — 0x05  (replaces v1 `SCAN` + repeated `D`)

* Args: `[window_ms u16 = 6000][max u8 = 10]`, both optional-carry by `LEN`.
* `RSP_OK` immediately (ditto v1 "queued"). For every tracker that answers the
  broadcast inquiry within the window, an `EVT_DISCOVERED` is streamed as the
  reply arrives. When the window elapses (or `max` uniques were seen):

  `EVT_DISCOVERY_DONE [count u8]`

* No polling. Duplicate UIDs are suppressed by the device.
* The window ends early when `max` is reached; `DISCOVERY_DONE` closes the scan
  in that case too.

### 4.6 `DISCOVER_CANCEL` — 0x06

* Args: none. `RSP_OK`; remaining events (if any) are followed by
  `EVT_DISCOVERY_DONE` with the partial count.

### 4.7 `PAIR` — 0x07  (direct manual pairing)

* Args: `[uid u32][freq u32][sf u8][bw u8]`
* `RSP_OK` once the pair command is queued to air (not when the tracker ACKs).
* Completion is signalled asynchronously:

  ```
  EVT_PAIR_STATUS [uid u32][status u8][rssi i8]
  status: 0 = ACKED ..., 1 = TIMEOUT, 2 = REJECTED
  ```

* On `ACKED` the GS switches radio to the given channel BEFORE the event
  (channel-first rule, §7.2) — the host must not re-issue `RADIO_CONFIG`.
* Calling `PAIR` for a currently-subscribed UID that is already paired is
  `RSP_ERR BLOCKED`.

### 4.8 `SUBSCRIBE` — 0x09  (the headline change; replaces 1 Hz `R` polling)

* Args: `[uid u32][flags u8]`
  * bit 0 `AUTO_PAIR` — if the UID is paired with a channel known to this GS or
    an outstanding pairing can be replayed, reuse it; otherwise it is exactly
    `PAIR` followed by telemetry events (host sees one op only).
  * bit 1 `LOCAL_FIX_STREAM` — reserved (see §6.3), ignored by firmware v2.0.
  * bits 2–7 reserved/zero.
* `RSP_OK` when the subscription table row is created (not when air handshake
  completes — flow of pair status is via `EVT_PAIR_STATUS`). `RSP_ERR` if
  the table (max 4 rows) is full.
* From then telemetry and state arrive as `EVT_TELEMETRY`, `EVT_OFFLINE`,
  `EVT_PAIR_STATUS`. The host does not poll.
* Subscribing twice to the same UID replaces the record (`RSP_OK`).

### 4.9 `UNSUBSCRIBE` — 0x0A

* Args `[uid u32]`. `RSP_OK` (a nonsubscribed UID is `RSP_ERR NO_SUB`, no radio
  side effects). Radio channel is *not* restored to discovery automatically,
  matching v1 §6 Step 9 behaviour; the host does `RADIO_CONFIG` explicitly.

### 4.10 `SUBS_READ` — 0x0B

* `RSP_OK`: `[n u8]` then n records of `[uid u32][flags u8]` (0 ≤ n ≤ 4).

### 4.11 `SEND_AIR` — 0x0C  (replaces v1 `T`)

* Args: `[dst u32][air_type u8][air_payload up to 24 bytes][air_payload_pad as needed]`
* `RSP_OK` when queued. The device builds the air frame of §6: `[our_uid][dst][air_type][len][payload]`.
* E.g. asking tracker `a1b2c3d4` for voltage:
  `SEND_AIR dst=a1b2c3d4, air_type=VOLT_REQ(5), payload=∅`.

### 4.12 `VOLTAGE_REQ` — 0x0D  (shorthand for `SEND_AIR` type 5)

* Args `[uid u32]`. Reply: `EVT_VOLTAGE [uid u32][mv u16][rssi i8]`.

### 4.13 `SETTINGS_GET` — 0x0E

* `RSP_OK`: `[n u8]` then n × `[label s8][value u32]`.
* Label set & defaults per §8.

### 4.14 `SETTINGS_SET` — 0x0F

* Args `[label s8][value u32]`. `RSP_OK` after immediate application for radio-able
  labels (radio re-applied), or just stored for non-radio ones. Invalid label is
  `RSP_ERR BAD_ARG`. Persistence is **not** implicit (§8).

### 4.15 `SETTINGS_SAVE` — 0x10 / `SETTINGS_RELOAD` — 0x11

* Available only if `caps` bit 2 is set (i.e. compiled with the flash store).
  `RSP_OK` or `RSP_ERR NOT_SUPPORTED`.

### 4.16 `REBOOT` — 0x12 / `FACTORY` — 0x13

* `RSP_OK` is sent, TX ring is flushed, then reset (usually ~100 ms; `FACTORY`
  also erases the settings page when persistence is compiled in).

### 4.17 `DEBUG_MODE` — 0x14

* Args `[on u8]`. `RSP_OK`; from then on (until reboot) the device may emit
  `DBG` frames with ASCII log lines. Every `DBG` frame is *optional* — hosts
  must not rely on them.

---

## 5. Sequence, timeout and flow rules

* One `CMD` in flight; `SEQ` is the only correlation key. The host assigns it;
  the device echoes it in the terminal frame.
* Host default: 5 000 ms response timeout (matches device worst-case blocking
  v1 budget); on timeout the host resets its TX state, marks the device
  *unresponsive* and re-`HELLO`s at a later cadence (§5.4) rather than
  spontaneously hammering the link.
* `EVT` frames do not pause commands (they come from the same serial pipe but
  the event queue is flushed from the main loop and the 64-byte EP FIFO drains
  in ~milliseconds at FS speed).
* Drop/eviction policy: the device drops events it cannot queue; the next frame
  the host sees for that path carries that that burden in `EVT_OVERFLOW` (§6.1).
* Host reconnect procedure (latency ≤ 200 ms): no-op beyond `HELLO` again; any
  stale subscriptions the device carried are reported by `SUBS_READ`'s table.
  Re-appearance after a fix is implied by the next `EVT_TELEMETRY`.

### 5.4 Host-state machine (all clocks in the *host*)

```
DISCONNECTED → (open port, 2 s) → HELLO
HELLO fails  → retry ≤ 3×, then mark not-a-v2 device
HELLO OK     → OPERATIONAL
                 ├ DISCOVER → EVT_DISCOVERY_DONE (≤ 6 s + margin 1.5 s)
                 ├ SUBSCRIBE flow (pair if needed): RSP_OK → EVT_PAIR_STATUS(≤ 8 s)
                 └ steady state: EVT_TELEMETRY cadence
```

No host-side timers other than the two slow ones above (`DISCOVERY_DONE`
dead-poll, pair/status watching with an explicit TIMEOUT event).

---

## 6. USB event catalogue

Payload of an `EVT` frame is `[ev u8][data...]`.

### 6.1 Event list

| id | Name | Payload | When |
|---|---|---|---|
| `0x01` | `EVT_DISCOVERED` | `[uid u32][rssi i8]` | Per unique tracker response during a DISCOVER window |
| `0x02` | `EVT_DISCOVERY_DONE` | `[count u8]` | Window end (natural, `max` reached, or CANCELLED) |
| `0x03` | `EVT_PAIR_STATUS` | `[uid u32][status u8][rssi i8]` | Pair handshake result; radio channel switch is reported with status ACKED |
| `0x04` | `EVT_TELEMETRY` | `[uid u32][rssi i8][pkt 19B]` | Telemetry packet from any subscribed UID |
| `0x05` | `EVT_VOLTAGE` | `[uid u32][mv u16][rssi i8]` | Response to `VOLTAGE_REQ` / air VOLT_REP |
| `0x06` | `EVT_LORA_RX` | `[uid u32][rssi i8][len u8][payload]` | Any air frame from a *non-subscribed* UID that is not a REGISTERED message type |
| `0x07` | `EVT_OFFLINE` | `[uid u32][age_s u16]` | A subscribed UID has not been heard for 10 s (matches LED "no tracker" criterion) |
| `0x08` | `EVT_OVERFLOW` | `[dropped_events u16][last_evt_seq u16]` | Events were evicted; monotonic event counter continuing after eviction |
| `0x09` | `EVT_LOCAL_FIX` | `[pkt 19B]` | GS's own GPS fix; rate ≈ tracker cadence, emitted only if `caps` bit 3 and host turned it on |

### 6.2 Event timing notes

* `EVT_TELEMETRY` cadence is chosen by the tracker (~2 s when paired; §7.3).
  The GS does not throttle it; the host may throttle its rendering.
* `EVT_OFFLINE` is evaluated in the main loop (not an RTOS timer), so it is
  opportunistic but bounded by roughly the loop tick granularity.

---

## 7. Air plane

### 7.1 Channel model

* **Discovery channel** = the device-wide default radio params (§9). Discovery,
  unpaired-listening trackers, and unpairing "return" commands all happen here.
* **Dedicated (pairing) channel** = arbitrary frequency/params assigned at pair
  time. One tracker ↔ one GS per channel; a tracker paired to a channel never
  answers broadcasts on the discovery channel again (v1 semantics) until
  reverted (REBOOT, or explicit unpair command — out of scope for v2.0).

### 7.2 Channel-first rule

`PAIR` — whether issued directly (`PAIR`) or implicitly via `SUBSCRIBE flags
AUTO_PAIR` — never switches the GS radio until the tracker's `PAIR_ACK` air
frame is received on the *discovery* channel. The GS then applies the new
params, returns to receive, and emits `EVT_PAIR_STATUS (ACKED)`. The host never
needs a "we moved" notification beyond that.

### 7.3 Telemetry cadence

Tracker paired → transmits a `TELEMETRY` air frame once every *other* fix
(~2 s cadence at 1 Hz fix rate), unchanged from v1.

### 7.4 Air frame format

Binary, little-endian. The SX1262 LoRa **packet CRC16 is enabled** (v1 disabled
it), which is free error detection without payload cost — so there is no
application-level air CRC.

```
full frame (addressee required):
  [src u32][dst u32][type u8][len u8][payload len bytes]

compact frame (only for one-to-one tracker→GS flights, type ≥ 0x20):
  [src u32][type u8][len u8][payload len bytes]
```

* `dst = 0xFFFFFFFF` ⇒ broadcast.
* `len` ≤ 24.
* Compact form (`dst` omitted) is only legal for types ≥ 0x20 from a tracker to
  a GS on a dedicated channel. The GS still records `src` — good for logs.

### 7.5 Air message types

| type | Name | Src→Dst | Payload | Purpose |
|---|---|---|---|---|
| `0x01` | `INQ_REQ` | GS → broadcast | — | "Who is out there?" (replaces v1 `U` request) |
| `0x02` | `INQ_REP` | tracker → GS | — | Reply to `INQ_REQ`; `src` *is* the UID (replaces v1 `U` reply) |
| `0x03` | `PAIR_CMD` | GS → tracker | `[freq u32][sf u8][bw u8]` | Command config switch (replaces v1 `T<freq>,<sf>,<bw>`) |
| `0x04` | `PAIR_ACK` | tracker → GS | — | Acknowledge `PAIR_CMD` (replaces v1 `TACK`) |
| `0x05` | `VOLT_REQ` | GS → tracker | — | Request battery voltage (replaces v1 `V` request) |
| `0x06` | `VOLT_REP` | tracker → GS | `[mv u16]` | Battery voltage reply (replaces v1 `V<mv>`) |
| `0x07` | `NAK` | any → initiator | — | Unknown `type` reply. Sent on the channel the request arrived on, no delay |
| `0x20` | `TELEMETRY` | tracker → GS (compact) | `GpsPacket_t` (19 B) | GPS/repeated fix data (replaces raw-binary v1 gps frame, with no dst field) |

### 7.6 Air sizing summary (approximate)

Using v1's ASCII air format vs v2 binary (same radio settings):

| Air message | v1 bytes | v2 bytes | v2 with SX1262 CRC is strictly shorter |
|---|---|---|---|
| INQ_REQ broadcast | 19 | 10 | ~47 % |
| INQ_REP | 27 | 10 | ~63 % |
| PAIR_CMD | 19+ | 15 | ~21 %+ |
| PAIR_ACK | 22 | 10 | ~55 % |
| VOLT_REQ | 19 | 10 | ~47 % |
| VOLT_REP | 22 | 12 | ~45 % |
| NAK | 3 | 10 | longer, acceptable (v1's NAK had no uid prefix; §9 bug) |
| TELEMETRY | 28 | 25 | ~11 % (and it is structured) |

Sized deltas matter during discovery (n replies inside a bounded window) and
for the respected duty-cycle budget on air.

### 7.7 Discovery / arbitration timing (unchanged from v1, restated for completeness)

| Parameter | Value |
|---|---|
| Discovery window | 6 000 ms |
| Response slot range | 0–5 000 ms in 500 ms slots (11 slots) |
| RX timeout | 5 000 ms |
| TX complete budget | 1 500 ms |

---

## 8. Settings (defaults)

Volatile in the basic build; persisted in the `SETTINGS_PERSIST` build.

| Label (ASCII s8) | Setting | Default |
|---|---|---|
| `f` | Frequency, Hz | 434 000 000 |
| `s` | Spreading factor enum | 9 |
| `b` | Bandwidth enum | 3 (62.5 kHz) |
| `c` | Coding rate enum | 1 (4/5) |
| `d` | TX power, dBm | 22 |
| `o` | Overcurrent, mA | 150 |
| `p` | Preamble length, symbols | 8 |
| `m` | Boot mode | 0 (ground station) — *reserved; firmware v2 boots GS and goes to tracker mode on air command only, matching shipped v1 behaviour. Bench/tracker units are flashed with `-DAPP_BOOT_MODE=APP_BOOT_MODE_TRACKER` (compile-time; see the migration plan "Boot mode policy").*

---

## 9. LoRa parameter enum (unchanged values from v1 §4)

Bandwidth: `0`=7.8k, `1`=15.6k, `2`=31.25k, `3`=62.5k *(default)*, `4`=125k,
`5`=250k, `6`=500k. Spreading factor: 5–12. Coding rate: 1=4/5 … 4=4/8.
TX power: −9…22 dBm.

---

## 10. Telemetry packet decode (authoritative; from `GPS_BINARY_PACKET.md`)

```
GpsPacket_t (19 bytes, packed, little-endian on air):
 0  u32  time_s     seconds since midnight UTC (0..86399)
 4  i32  latitude   ddmm.mmmmm × 100 000, sign: + = North
 8  i32  longitude  dddmm.mmmmm × 100 000, sign: + = East
12 i16  altitude   metres MSL
14 i16  geoid_sep  decimetres
16 u8   satellites
17 u8   hdop       ×10
18 u8   flags      bits 2..0 = fix quality (0..6), 7..3 reserved
```

Decode to engineering units (host side):

```
lat_deg  = trunc(lat / 100000 / 100) + ((lat / 100000) % 100 + (lat % 100000)/1e5) / 60   (signed)
lon_deg  = trunc(lon / 100000 / 100) + ((lon / 100000) % 100 + (lon % 100000)/1e5) / 60   (signed)
alt_m    = altitude
geoid_m  = geoid_sep / 10
hdop     = hdop / 10
```

---

## 11. Worked session example

Annotated (host log, one command in flight; `→` host Tx, `←` device Rx):

```
→ CMD  SEQ1  HELLO
← RSP_OK SEQ1   fw 2.0, caps 0x000F, uid 1a2b3c4d

→ CMD  SEQ2  DISCOVER window=6000 max=10
← RSP_OK SEQ2
← EVT DISCOVERED uid a1b2c3d4 rssi −89          (t ≈ +1.3 s, random slot)
← EVT DISCOVERED uid e5f6a7b8 rssi −72          (t ≈ +2.9 s)
← EVT DISCOVERY_DONE count=2                    (t ≈ +6.0 s)

→ CMD  SEQ3  SUBSCRIBE uid=a1b2c3d4 flags=AUTO_PAIR
← RSP_OK SEQ3
← EVT PAIR_STATUS a1b2c3d4 ACKED rssi −81       (tracker acked; GS switched radio)

… every ~2 s, no commands from host:
← EVT TELEMETRY a1b2c3d4 rssi −78 [19 B GpsPacket_t]
← EVT TELEMETRY a1b2c3d4 rssi −77 [19 B GpsPacket_t]

→ CMD  SEQ4  VOLTAGE_REQ uid=e5f6a7b8
← RSP_OK SEQ4
← EVT VOLTAGE e5f6a7b8 mv=3850 rssi −84

… after 10 s of no telemetry (e.g. tracker lost):
← EVT OFFLINE a1b2c3d4 age=12
```

Total host *commands* for a full discovery→tracking session (previously
`UID + SCAN + ~4 × D + PAIR + ~1 Hz R + L`): **5, and zero steady-state polling.**

---

## Appendix A — Wire test vectors

Frames live for conformance tests on both sides (device + host). All values hex,
little-endian; CRC is CCITT-FALSE. These vectors were generated and CRC-checked with the
reference encoder (`tools/v2bench.py`); a conforming decoder must accept them,
and re-encoding must reproduce them byte-for-byte.

```
# 1) HELLO cmd, SEQ=1                                        11 bytes
A5 5A 02 01 01 00 01 00 00 EC 49

# 2) HELLO rsp, SEQ=1  fw 2.0 caps 0x000F uid 1a2b3c4d       18 bytes
A5 5A 02 02 01 00 08 00 02 00 0F 00 4D 3C 2B 1A 4C 58

# 3) INFO cmd, SEQ=2                                         11 bytes
A5 5A 02 01 02 00 01 00 01 1F B7

# 4) DISCOVER cmd, SEQ=3  window=6000 max=10                 14 bytes
A5 5A 02 01 03 00 04 00 05 70 17 0A A7 45

# 5) SUBSCRIBE cmd, SEQ=4  uid a1b2c3d4 flags=AUTO_PAIR      16 bytes
A5 5A 02 01 04 00 06 00 09 D4 C3 B2 A1 01 2B 07

# 6) EVT_DISCOVERED, event SEQ=4  uid a1b2c3d4 rssi −72      16 bytes
A5 5A 02 04 04 00 06 00 01 D4 C3 B2 A1 B8 95 B5

# 7) EVT_TELEMETRY, event SEQ=5  uid a1b2c3d4 rssi −72, zero pkt  35 bytes
A5 5A 02 04 05 00 19 00 04 D4 C3 B2 A1 B8 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 B8 75

# 8) RSP_ERR, SEQ=9, code=BAD_ARG(3)                         11 bytes
A5 5A 02 03 09 00 01 00 03 E2 F0
```

The air vectors below use the full frame layout; the TELEMETRY row demonstrates
the compact form.

Air test vector (frame = `[src u32][dst u32][type u8][len u8][payload]`, no air CRC):

```
GS 1a2b3c4d pairs tracker a1b2c3d4 → 434500000/SF9/BW62.5 (0x03):
  4D 3C 2B 1A | D4 C3 B2 A1 | 03 | 06 | A0 F1 E5 19 09 03   = 15 bytes
Tracker ACKs:
  D4 C3 B2 A1 | 4D 3C 2B 1A | 04 | 00                        = 10 bytes
Tracker telemetry (compact, src only):
  D4 C3 B2 A1 | 20 | 13 | [19 B GpsPacket_t]                 = 25 bytes
```

---

## Appendix B — CRC16-CCITT (nibble table, 16 entries)

```c
static const uint16_t crc_tab[16] = {
  0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x6CC6,0x7CE7,
  0x914E,0x816F,0xA108,0xB129,0xC158,0xD139,0xE2DA,0xF2FB
};

uint16_t crc16_usb(const uint8_t *p, uint16_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*p++) << 8;
    uint8_t shift = 0;
    do {
      crc = (crc << 4) ^ crc_tab[(crc >> 12) & 0x0F];
    } while (++shift < 2);
  }
  return crc;
}
```

This is the bit-serial semantics of CCITT-FALSE (poly 0x1021, MSB first) in
table form. ~60 B flash. Implementation choice: the 256-byte table version is
allowed if space permits; correctness (given the test vectors) is the gate.

---

## Appendix C — Out of scope (v2.0)

* Multi-tracker coexistence on one channel (requires full CSMA/backoff, not
  mentioned in v1 air protocol).
* Encryption / authentication of telemetry.
* Over-the-air firmware update.
* Mobile (Android) transport specifics (uses the same binary serial layer).
