# V2 Protocol Migration Plan

Status: **Plan — not yet implemented.**

Companion to `docs/COMMUNICATION_SPEC_V2.md` (the V2 wire spec). This document
is the step-by-step implementation plan for both sides of the system:

* **Firmware** (`SimpleTracker/firmware`, STM32F042G6 — 32 KB flash / 6 KB RAM)
* **Java ground station** (`SimpleSuite/SimpleTrackerJava`)

Every hardware step ends with a **bench verification** using two physical
tracker units over USB:

* **Unit A** = ground station under test, connected to the PC (`Unit A`, GS role).
* **Unit B** = tracker (GPS attached, battery powered), boots in tracker mode
  (see "Boot mode policy" below), on the bench or on its charger.

The current firmware binary boots every unit as a ground station (audit M1),
so unit B on the bench needs a compile-time boot-mode switch until settings
persistence returns in Phase 1 (which makes `m` meaningful again).

---

## 0. Size budget (gates, measured on the linked ELF with `arm-none-eabi-size`)

**Re-baselined 2026-10-25 after a clean rebuild.** The original margins in this
planning section were extrapolated from `build/firmware.elf` dated May 2026,
which predated the September `main.c` / ublox-M10 changes; that ELF was
~1.15 KB flash / ~1.33 KB RAM smaller than the true image. The numbers below
are from a verified clean build of the current tree
(`arm-none-eabi-gcc 12.2.1`, `make clean && make`, 0 errors).
Still: treat every value as an orientation and re-measure at each step —
the per-step `arm-none-eabi-size` gate is the binding one.

| Milestone | Flash used (budget) | RAM static+stack | Free flash |
|---|---|---|---|
| Baseline (measured 2026-10-25) | 31 192 B actual (95.2 %) | 5 352 B (4 328 static + 1 024 stack; **792 B unreserved**) | 1 576 B |
| After Phase 1 prune | ≈ 30 300 B | ≈ 3 650 B | ≈ 2 450 B |
| After Phase 2 (USB V2 in, v1 ASCII parser + NMEA unpack **deleted**) | ≤ 30 600 B | ≤ 4 400 B | ≥ 2 150 B |
| After Phase 3+4 (air V2 in, v1 air shim out) | ≤ 30 000 B | ≤ 4 300 B | ≥ 2 750 B |
| After Phase 5 (raw ADC, raw UART — now optional deep prune) | ≈ 27 000 B | ≤ 4 200 B | ≈ 5 700 B |

Three structural consequences of the re-baseline (decisions, not just notes):

1. **The v1 ASCII interpreter and `gps_unpack_to_nmea` move from Phase 5 into
   Phase 2.0** (deletion). Budget math shows the USB-plane V2 cannot fit
   alongside them: `31 192 + ~2.6 K (v2 USB) > 32 768` unless the ~1.5 KB
   parser and ~0.9 KB NMEA packer go first. GPS air packing
   (`gps_pack_packet`) and the GpsPacket_t decode remain — only the USB-R
   NMEA re-serialisation dies.
2. Phase 2 consequently ends support for the v1 USB plane (the old smoke
   transcripts from Phase 0.2 are the anchor; after Phase 2 A is verified
   only via `v2bench`).
3. RAM: the freed ~1.7 KB (ST PCD handle + ST-CDC buffers) funds the v2 TX
   ring + event table; the **792 B of unreserved stack headroom** is thin —
   keep an eye on `._user_heap_stack` (`_Min_Stack_Size = 0x400`) whenever
   adding buffers, and prefer ring/table shrink over stack growth.

Hard rule: **a phase is not "done" until the gate holds and the bench test
matrix for that phase passes.** If a gate fails, stop and re-cut scope (the
Phase 5 items raw ADC / raw UART are the designated emergency reserves).

Record every measurement in the commit message of the phase, e.g.
`size: 28 876 flash / 2 244 ram (phase 1 gate: ≤ 28 900 ✓)`.

### Boot mode policy

v1 hard-codes `appContext.mode = MODE_GROUND_STATION` (`main.c:182`) because a
unit charging over USB must never transmit (commit `402ae89` keeps that).

* **v2.0**: kernel-of-difference is a single compile-time definition:
  `-DAPP_BOOT_MODE=APP_BOOT_MODE_TRACKER` (Makefile default: `GROUND_STATION`).
  `main.c` uses it for `appContext.mode`. Units flashed for bench/tracker duty
  use the tracker value. No runtime cost (< 0 B diff).
* **v2.1 (follow-up, not this plan)**: restore settings persistence (Phase 1
  restores the filesystem) and honor `SET m 1` at boot **only when the unit is
  not USB-powered** (defer to the existing anti-charging guard: `m` is read
  before USB enumeration starts, and a USB-vbus interrupt within the first
  500 ms forces GS mode).

---

## Phase 0 — Host harness and baseline audit trail (**no hardware needed**)

### 0.1 `tools/v2bench.py`

Python 3 + `pyserial` reference codec for both planes, implemented directly from
`docs/COMMUNICATION_SPEC_V2.md`:

* `crc16(buf)` (CCITT-FALSE, per Appendix B) + `encode`/`decode` of USB frames.
* `--self`: reproduce every Appendix A vector byte-for-byte, exit non-zero on
  mismatch. CI-able without hardware.
* `--port COMx --interactive`: opens the GS unit, sends hand-built frames,
  prints every decoded frame (with delta-t) as it arrives; supports
  `cmd NAME [args...]` conveniences for all §4 opcodes.
* Air-plane helpers: `air encode/decode` for §7.4 frames (for cross-checking
  while debugging the firmware air path).
* `--v1smoke`: helper that speaks the **v1 ASCII** protocol (`UID`, `C`, `SCAN`,
  `D`, `PAIR`, `R`, `L`, `V`, `I`) so pre-V2 phases can be verified without the
  Java app.

**Verify:** `python3 tools/v2bench.py --self` passes (CRC + vectors).
Gate: none (host-side).

### 0.2 Baseline bench capture

* Unit A → PC. `v2bench --v1smoke` runs: `V`, `I`, `SET f`, verdict log.
* Unit B (tracker boot) → power on next to unit A, air-visible.
* Run through the v1 happy path **once and record the transcript** as the
  migration's regression baseline: `SET f 434000000`, `SCAN` (sees B), `PAIR B`…
* Goal: a known-good v1 transcript to diff phases against, especially Phase 1
  where v1 semantics must be preserved exactly.

**Verify:** transcript saved to `tools/bench/v1-baseline.txt`.

---

## Phase 1 — Dead-code pruning, v1 semantics unchanged (flash + RAM recovery)

Each step = one commit, build + `arm-none-eabi-size`, then the same v1 smoke
test over USB. No v1 spec/behaviour changes in this phase (except H2, which is
a bug fix, and M2/§, which is a *decision*, documented in the v1 spec text).

### 1.1 Remove the ST USB device stack remnant (~−450 B flash, ~−1.7 KB RAM)

* `usb.c`: delete `MX_USB_PCD_Init()` and the `hpcd`/`HAL_PCD` region
  (TinyUSB's `dcd_stm32_fsdev` drives the peripheral itself); delete the legacy
  ST-CDC buffers (`UserRxBufferFS`, `UserTxBufferFS`, `usb_state_t`) and their
  `usb.h` declarations; keep `usb_isr`/NVIC wiring but confirm the IRQ shares
  the correct priority with TinyUSB's expectations.
* Makefile: drop `stm32f0xx_hal_pcd*.c`, `stm32f0xx_ll_usb.c` if nothing else
  references them; delete `USB_DEVICE/App/usbd_cdc_if.c` (not compiled).
* `stm32f0xx_hal_conf.h`: `#undef HAL_PCD_MODULE_ENABLED`.

**Verify (unit A alone, v1):**
Enumerates (`lsusb` shows the device, `dmesg` shows cdc_acm attach);
`v2bench --v1smoke unit-A` answers `V`, `I`, `SET f` correctly. Repeat the plug
3× (cold enumeration check).

Gate: flash ≤ 30 850, RAM static+stack ≤ 3 700. *(± estimated; if RAM does not drop ~1.7 KB
the legacy buffers were still referenced somwhere — grep before blaming the BSP.*

### 1.2 Fix `print()` truncation (**H2** — bug fix, tiny: ~−0/+50 B flash)

* `usb.c print()`: honoure `tud_cdc_write()`'s return value; buffer remaining
  bytes and flush on subsequent loop ticks until drained (or use `tud_cdc_write_flush()`
  on full drain). Replace with a loop that writes from the remaining slice; do
  not busy-wait.
* Also raise `CFG_TUD_CDC_TX_BUFSIZE` 64 → 256 in `tusb_config.h`
  (RAM cost +192 B, pays for itself in Phase 2's TX ring).

**Verify (unit A, v1 + unit B powered):**
`R` response against unit B (RSSI + GPS fields ⇒ > 64 B in v1 encoding) must be
byte-complete in `v2bench` output; `D` after a `SCAN` that found unit B must
list B fully. Regression-capture: diff the two transcripts' `R` line to
the Phase-0 baseline (baseline likely truncated — the fix makes it *longer*;
sanity-check field count instead of byte-identical).

Gate: flash ≤ 30 900, RAM static+stack ≤ 3 900 (+192 TX buffer).

### 1.3 Filesystem decision: **delete** (~−760 B flash)

* Delete `filesystem.c/.h`, `flash.c/.h`, `fs_read_config`/`fs_write_config`
  prototypes; `main.c` drop-in of the commented `fs_init`/`load_settings`
  blocks (lines 171–173, `load_settings()` at 93–100); `setting.c` falls back
  to the compiled defaults table (which already carries `DEFAULT_BW = 62.5k`).
* `stm32f0xx_hal_conf.h`: drop `HAL_FLASH_MODULE_ENABLED` only if v2 needs
  nothing else on flash (kept off until something re-enables it).
* **Decision recorded here:** v2.0 ships without persistence; `SETTINGS_SAVE`/
  `SETTINGS_RELOAD` (§4.15) return `RSP_ERR NOT_SUPPORTED` and `caps`
  bit 2 = 0. A future `SETTINGS_PERSIST` build re-adds a minimal flash writer
  on top of HAL_FLASH directly (skip the v1 filesystem format entirely).
* Update **both** v1 spec copies' §2.9 wording to "settings volatile in the
  firmware as shipped from commit X onwards".

**Verify (unit A, v1):** `SET b 2` then power-cycle → back to default; `SCAN`
still works; `--v1smoke` transcript healthy.

Gate: flash ≤ 30 150. (filesystem.o only ~260 B here; the real win is *not* growing this area back later)

### 1.4 Remove TIM16 init (~−100 B flash)

`tim.c` + `MX_TIM16_Init()` call in `main.c:166`; grep confirms `htim16` is
write-only. LED is GPIO-only (`led.c` uses no TIM).

**Verify:** unit A boots, LED idle blink sequence runs (visual), v1 smoke
passes.

Gate: flash ≤ 30 100, RAM unchanged.

### 1.5 (Optional, cheap) v1 M3 fix while the ASCII parser still exists

* `set_config`/`get_config` NULL/short-token guards (`commands.c`): send
  `"NAK"` (existing v1 style, still spec-legal at this point) instead of
  array-indexing NULL. ~40 B. Cheap insurance while the parser is alive;
  the parser dies in Phase 5 anyway.

**Verify:** `SET` (no args) and `SET f` over USB → `NAK` instead of a hang/0-Hz.

Gate unchanged.

---

## Phase 2 — USB-plane V2 (device speaks v2 bytes; air plane still v1)

Everything in §3–§6 of the V2 spec, machine-tested, while the air plane keeps
its v1 ASCII format. This is the largest firmware diff; it is **staged in
sub-steps with the hard rule that nothing on-air changes in this phase**, so
unit B can stay on the v1 firmware throughout.

### 2.0 Delete the v1 ASCII interpreter + USB NMEA path (added after re-baselining)

Frees the space Phase 2.1–2.5 needs. One commit covering:

* `command.c` (framer, 458 F / 393 R obj) + `commands.c` (handlers, 1 062 F /
  107 R obj) deleted, together with `cmd_add`/`cmd_read_input` wiring in
  `main.c`; `strtok_r`/`strtol`/`itoa` become unreferenced → libc shrinks.
* `gps_unpack_to_nmea` (912 B retained flash) deleted — it exists only to
  serve the v1 `R` response, which is going away with the parser. Note:
  `gps_pack_packet` (air side) and the `GpsPacket_t` decode stay.
* `usb.c` `print()` is now called only from the (new) v2 TX ring code path —
  keep the chunked-write fix from Phase 1.2 as the ring's flush helper.

**After this commit the unit-under-test responds only to garbage** until
Phase 2.1 lands, so do 2.0 and 2.1 back-to-back and flash once; between them,
bench verification is a compile + size check only.

Gate: flash ≤ 27 900 (assuming Phase 1 landed at ≈ 30 300; measured −1.5 KB
parser − 0.9 KB NMEA − ~0.2 KB misc).

### 2.0b (re-baselined gates for the sub-steps below)

### 2.1 Shared infrastructure (`v2proto.c/.h`, one new pair of files)

Independent sub-commits, each bench-verifiable in isolation:

* `crc16_usb()` per Appendix B. Verified by `v2bench --self` vectors through
  the device only later (no direct test exposure needed).
* RX state machine: sync scan `A5 5A 02`, LEN check, CRC check; drop +
  increment a drop counter on bad CRC/LEN/type. Max payload 128.
* TX ring (256 B, doubles as the H2 fix's buffer) with `tud_cdc_write()`
  return-aware flushing, plus the `EVT_OVERFLOW`-backed eviction rule (§2.2).
* Command context: one-in-flight busy flag + active SEQ.

**Verify (unit A alone):** `v2bench --port <A>` sends `HELLO` × 10 back-to-back
plus a deliberately corrupted frame sandwiched between good ones; expect
exactly 10 clean `RSP_OK`, no visible confusion. Hex-dump every byte both ways
into `tools/bench/phase2-framer.txt` for review.

Gate: cumulative ≤ 28 200.

### 2.2 Binary dispatcher + HELLO/INFO/RADIO_*

* `HELLO` (returns fw/caps/uid; caps = `0x0003` — EVENTS + SUBSCRIBE — until
  Phase 4 adds LOCAL_FIX_STREAM).
* `INFO`, `RADIO_CONFIG` (moves the v1 `C` logic verbatim: settings updated +
  radio re-init + re-enter RX), `RADIO_READ` (v1 `GET` equivalent).
* Pre-`HELLO` opcodes get `RSP_ERR NOT_SUPPORTED` (this is the "is this v2?"
  probe; v1 frames from an old host look like garbage and are dropped by the
  CRC, which is the correct behaviour).

**Verify (unit A alone):** `v2bench`: `HELLO` → payload matches §4.1 layout;
`RADIO_CONFIG 434500000 9 3 1 22` → immediately after `RSP_OK`, `RADIO_READ`
echoes the new params; `SET f` (ASCII, v1) after `HELLO` → NOT_SUPPORTED;
`RADIO_CONFIG sf=4` → BAD_ARG. Vectors: send Appendix A vectors 2–4, expect
my expected byte ranges back (except RSSI/CRC-nondeterministic fields which
assert structurally not byte-wise).

Gate: cumulative ≤ 28 700.

### 2.3 Subscriptions skeleton + DISCOVER (**air still v1**)

* Subscription table (4 rows, uid/flags), SUBSCRIBE/UNSUBSCRIBE/SUBS_READ.
* `DISCOVER`: sends v1 air `SCAN`-equivalent (`&<gs_uid>U` broadcast);
  parses v1 air replies (which are `&<uid>U`-style) → builds `EVT_DISCOVERED`;
  6 s slot-timeout window → `EVT_DISCOVERY_DONE`.
* `VOLTAGE_REQ`: v1 air `&<uid>V` → parses v1 `&<mv>` Δ → `EVT_VOLTAGE`.

**Verify (unit A + unit B powered):**
1. B on (tracker boot). A: `DISCOVER` → `RSP_OK`, `EVT_DISCOVERED uid=B`,
   then `EVT_DISCOVERY_DONE count=1`. Δ → record timings.
2. `SUBSCRIBE uid=B flags=0` → `RSP_OK`, `SUBS_READ` → 1 row.
3. `UNSUBSCRIBE uid=B` → `RSP_OK`, `SUBS_READ` → 0 rows.
4. `UNSUBSCRIBE uid=B` again → `RSP_ERR NO_SUB(7)`.
**Verify (air off / radio unplugged):** `DISCOVER` → `DISCOVERY_DONE count=0`
after ~6 s (+≤ 100 ms jitter — measure actual).

Gate: cumulative ≤ 29 200.

### 2.4 Pairing via v1 air pair (SUBSCRIBE AUTO_PAIR + PAIR + PAIR_STATUS)

* `PAIR uid freq sf bw` (§4.7) → v1 air `&B T<freq>,<sf>,<bw>`-equivalent +
  v1 pendingFreq/SF/BW logic + v1 TACK → `EVT_PAIR_STATUS`.
* `SUBSCRIBE AUTO_PAIR`: for a UID not yet paired, auto-runs the above with
  the current GS settings as the target channel.
* If B is already on a dedicated channel from a prior pair, `SUBSCRIBE
  AUTO_PAIR` with the same UID skips re-issuing (this matches v1: the GS
  tracks pendingFreq; v2 keeps it in the sub row).

**Verify (A + B):**
1. A `SUBSCRIBE B AUTO_PAIR` → `RSP_OK` → `EVT_PAIR_STATUS ACKED` (Δ ≤ 8 s,
   record). B's LED shows tracker-"paired" behaviour.
2. Within ~2–3 s: `EVT_TELEMETRY uid=B ...` begins streaming (v1 air payload
   decoded: v2 fills the same 19-byte GpsPacket_t structure locally from B's
   v1-format air bytes — same `gps_unpack` used by v1, only the USB side is
   new). Confirm ≥ 5 consecutive EVT_TELEMETRY without any host poll.
3. `RADIO_READ` reflects the dedicated channel (channel-first rule: GS
   switched after TACK).
4. B power off → `EVT_OFFLINE uid=B age≈10` within ~10–12 s, exactly once.
5. B power on → `EVT_TELEMETRY` resumes (B is still on the dedicated channel
   after reboot? No — B reboots into tracker boot on the discovery channel, so
   no more telemetry; then a fresh DISCOVER on A sees B again. Record which
   behaviour you observe; this is a known v1 edge and drives the v2.1
   unpair/persistent-pair work item described in Appendix B of the V2 spec).

Gate: cumulative ≤ 29 500.

### 2.5 DBG channel + DEBUG_MODE + REBOOT + factored settings stubs

* `DEBUG_MODE on` → `DBG` frames (`DBG <text>` v1 lines; keep ≤ 70 B/frame).
* `REBOOT` (`RSP_OK`, flush, wait 100 ms, NVIC_SystemReset()) and `FACTORY`
  (no persistence yet → reset only; caps bit 2 = 0 so `SETTINGS_SAVE`/`RELOAD`
  return NOT_SUPPORTED, `FACTORY` returns NOT_SUPPORTED too).
* `SETTINGS_GET` → returns the current settings table (the v1 defaults +
  any runtime `SETTINGS_SET` so far); `SETTINGS_SET label value` applies
  radio labels immediately (same path as v1 `SET`).

**Verify (unit A):** `DEBUG_MODE 1` → see DBG frames; `SETTINGS_SET b 2`, then
`SETTINGS_GET` shows `b=2`; `REBOOT` → after boot,
`RSP_ERR NOT_SUPPORTED`-on-first-non-HELLO then `HELLO` succeeds and
`b` is back to 3.

Gate: cumulative ≤ 30 600 bytes flash. **Phase-2 end gate.**

---

## Phase 3 — Air-plane V2 (both units replaced simultaneously = lockstep)

Now the binary air frames of V2 spec §7 replace the v1 ASCII air protocol.
Because both sides change at once, this phase must flash A and B **together**
from the same commit; keep the previous commit's binaries in
`tools/bench/phase3-rollback/` for the rollback case.

### 3.1 Air codec (`v2air.c`)

* §7.4 frame builder/parser: full frame `[src][dst][type][len][payload]` with
  `len ≤ 24`, dst broadcast; compact form for type ≥ 0x20.
* 1-byte decoder state (types may arrive mid-stream, restart cleanly on the
  radio's RX entry).
* NAK (type 7) emission on unknown types per §7.5 (fixes v1 audit M4:
  NAK now carries src/dst).

**Verify (no radio stage):** unit tests compiled in (temporary `#ifdef
V2AIR_SELFTEST`), exercised in the A-alone bench: send AIR vectors
(spec Appendix A air block) NAK/PAIR etc.; remove the `#ifdef` from the
shipping build after the bench pass.

Gate: +400 B.

### 3.2 LoRa CRC enable + discovery over air V2

* `lora_init`: `crcEnabled = 1` (both units — lockstep).
* Tracker's rx task: INQ_REQ → 500 ms-slot-keyed backlog, INQ_REP reply.
* GS discovery path: broadcast INQ_REQ, collect INQ_REP → `EVT_DISCOVERED` /
  `EVT_DISCOVERY_DONE` to the USB plane from §2.3.
* The v1 SCAN/D/PAIR/T/V air builders + parsers are now dead: delete them in
  this commit (`processLoRaRx` shrink ~1.6 KB combined), though callbacks stay.

**Verify (A + B, both v2):**
1. `DISCOVER` → B found (timings recorded; air time INQ_REP — should show
   fewer collisions: run 5× in a row and count success; log in
   `tools/bench/phase3-discovery.txt`).
2. `SUBSCRIBE AUTO_PAIR` → PAIR_CMD air → B switches channel → PAIR_ACK air →
   `EVT_PAIR_STATUS ACKED` → telemetry stream 25-byte TELEMETRY air frames.
3. Kill B briefly → `NAK` visible? No: A sends PAIR_CMD twice? CSMA… just
   record the raw behaviour on a channel sweep and validate nothing wedges.

Gate: ≤ 30 000 bytes flash (deletions fund the additions here).

### 3.3 VOLT_REQ/VOLT_REP + NAK on air

* Replace v1 V/V-reply paths with VOLT_REQ→VOLT_REP (air compact + payload).
* Unknown air type → NAK (framed). On the GS: a NAK from an unpaired tracker
  on the discovery channel is logged via `DBG` (not an event — it's
  host-triggered rarely); on unit B: a NAK is *sent*, never acted on.

**Verify (A + B):**
1. `VOLTAGE_REQ uid=B` → `EVT_VOLTAGE mv=<plausible>` (cross-check against the
   battery's DMM or charger telemetry; ±50 mV).
2. Narrow: pull unit GPS line while powered (bench tracker PCB rev permitting)
   set a VOLT_REQ on a substituted `air_type = 9` via `SEND_AIR dst=B air_type=9`
   → air NAK observed (poor man's fixture unless you have a sniffer; otherwise
   assert only that nothing hangs and `DBG` shows the NAK send).
3. `SEND_AIR dst=B air_type=PAIR_CMD payload=<garbage>` must be NAK'd by B
   (payload-length check) — B keeps telemetry streaming unaffected.

Gate: unchanged; **Phase-3 end gate.**

---

## Phase 4 — Telemetry events + NMEA removal

* `EVT_TELEMETRY` carries the packed GpsPacket_t verbatim (§10); `gps_unpack_to_nmea`
  and its buffers are already gone (Phase 2.0). This phase deletes the last
  memory ref (`lastGpsSentence`) and the v1 `L` response storage.
* `EVT_LOCAL_FIX`: A's own GPS streams when `HELLO` caps bit 3 advertised
  (keep cap off in v2.0; bit is defined in the spec but unimplemented —
  **decision**: implement LOCAL_FIX behind `SUBSCRIBE flags bit 1` only if
  flash budget says yes at this point; default: defer, keep the LSB of caps
  missing to keep room for Phase 5).
* `EVT_LORA_RX` for non-subscribed traffic (little used; implement only the
  paired-uid-not-subscribed case, 30 B).
* EVT_OFFLINE watcher task + EVT_OVERFLOW (drop-oldest event ring = 8, evicts
  oldest event, host sees `EVT_OVERFLOW`).

**Verify (A + B):**
1. `EVT_TELEMETRY` packet → decode in `v2bench` live: lat/lon in expected
   place '{' fix at bench coordinates; check `sat/hdop/alt` sanity.
2. `EVT_OFFLINE` after B power-off (repeat, `age` ~10 s, then telemetry
   disappears until next SUBSCRIBE cycle is fine).
3. EVT_OVERFLOW: hard to trigger at 1-subscriber cadence; optional by hand:
   subscribe B and quickly toggle `DEBUG_MODE 1` repeatedly while bursts —
   not required to pass; assert the path compiles and drop-oldest picks the
   oldest event instead of corrupting the ring (unit-level debug assert).

Gate: flash ≤ 30 000; **H3 closes** (no NMEA on device), air payload = 19 B
packet + 5 B header.

---

## Phase 5 — Space trim: optional deep prune (raw ADC / raw UART)

The v1 ASCII interpreter and `gps_unpack_to_nmea` were already deleted in
Phase 2.0. Remaining here, only if the Phase-3/4 end gates say the budget
needs it:

* Register-level ADC driver (~−1.0 KB); raw UART GPS driver (~−1.0 KB) —
  the designated emergency reserves from the size table. Touch the GPS byte
  path last and always bench-verify fixes immediately after (it is the one
  path whose failure mode "works on the bench, dies in the field").
* `DBG` machinery keeps a tiny number formatter (60-byte `u32→ascii`) —
  implemented in Phase 2.0 alongside the dispatcher.

**Verify (A + B):** Full end-to-end regression: `DISCOVER` → `SUBSCRIBE` →
telemetry 10 min; `VOLTAGE_REQ`; `REBOOT`; `RADIO_CONFIG` to 868 MHz and back
(select a channel that doesn't clash with other bench radios).
**Size gate: flash ≤ 27 000, RAM static+stack ≤ 4 300** (only pull the trim
items that re-open the needed margin: if reserve A alone suffices, skip the
UART driver).

---

## Phase 6 — Java ground-station migration (`SimpleSuite/SimpleTrackerJava`)

Replace, in the order given, so the code compiles after each step:

### 6.1 Frame codec + CRC (pure-JVM, unit-testable without hardware)

* `V2Codec` (Kotlin/Java): `decode(InputStream.int)`, state machine per §3;
  `encode(cmd/op, payload)`; unit tests = spec Appendix A vectors (host-decode
  direction + re-encode byte-equal).
* Air `GpsPacket` POJO with the §10 decode; unit tests with known triangles
  (reuse GPS_BINARY_PACKET.md sample bytes).

**Verify:** `./gradlew :core:test` green.

### 6.2 Transport replace: delete `LineFramer` + `SerialCommandQueue`

* Delete `platform/LineFramer.java` (the 50 ms flush hack) and
  `logic/SerialCommandQueue` (one-in-flight 5 s queue). Replace with:
  * `V2Framer`: reads bytes off the serial `InputStream` in an executor thread,
    emits `V2Frame` onto a `Flow<DispatcherQueue>`.
  * `V2Session`: `send(cmd): CompletableFuture<Rsp>` keyed on SEQ, one-in-flight
    guard, 5 s timeout, CONNECTED state machine per §5.4 (HELLO → OPERATIONAL;
    every downstream op asserts OPERATIONAL).
* Wiring: `SerialHandler` keeps `jSerialComm` open/close; everything above it
  is new code.

**Verify (A alone):** a tiny `tools/jtalk` CLI (`main`) drives `V2Session`:
HELLO, INFO, RADIO_CONFIG, SUBS_READ. Recorded transcript.

### 6.3 Event-driven `TrackerProvider`

* Replace `trackingStepRemote`/`trackingStepLocal`/`discoveryPoll` timers with
  event renders: `V2Frame.EVT → domain` mapping (TELEMETRY→`TrackerState` fix,
  OFFLINE→`TrackerState.offline=true`, PAIR_STATUS→pairing state, VOLTAGE→
  battery, DISCOVERED→discovered set, DISCOVERY_DONE→scan finished).
* Rewrite `TrackerState` to be the single state-holder instead of the
  `lastRawRemote` string; remove that hack.
* `DiscoveredTracker`/`DetectedTracker` become simple records injected from the
  event path; UI re-renders on the StateFlow, host UI throttle stays.

**Verify (A + B, v2 firmware):** run the full app: open port → OPERATIONAL
banner; hit discover → B pops up with RSSI; subscribe → fixes appear at ~1 Hz
(`Δ` from telemetry), B power-off → tracker row dims/stale — compare behaviour
against the pre-migration screen captures.

### 6.4 Pairing + voltage + config UI

* PAIR flow → `EVT_PAIR_STATUS` tempo; AUTO_PAIR in SUBSCRIBE replaces the
  manual "PAIR then poll until telemetry" sequence.
* Voltage widget hooks `EVT_VOLTAGE` (requests VOLTAGE_REQ on an interval —
  30 s, not 2 s).
* Global config panel writes `SETTINGS_SET` + RADIO_CONFIG.

**Verify (A + B):** pair a fresh-rebooted B end to end from the UI; unplug the
DTR drop-reconnect: unplug.single deliberate cable-unplug → app HELLOs again
within ≤ 3 s; subscriptions reapplied from device table (`SUBS_READ`).

### 6.5 Remove v1 remnants

* Delete `GpsParser.java` and the rest of the NMEA parsing path kept alive by
  `L`/GPS-sentence path (device no longer sends NMEA); keep the class only if
  TS parser of user-supplied GPX files uses it (check `import` graph).
* Delete `-D` serial flush workarounds from README; update `docs/README`
  + protocol tables.

**Verify:** full regression matrix + `./gradlew clean build`.

---

## Phase 7 — Cutover, spec consolidation, commit

1. **Consolidate specs**: keep `COMMUNICATION_SPEC_V2.md` as the single live
   spec; move `firmware/COMMUNICATION_SPEC.md` → `docs/COMMUNICATION_SPEC_v1.md`
   marked "SUPERSEDED — historical"; delete `docs/COMMUNICATION_SPEC.md`
   (the drift-prone divergent copy). Audit-check one last time that `README`
   and `GPS_BINARY_PACKET.md` reflect v2 defaults (BW 62.5 kHz ✓ already
   applied).
2. **Commit the audit document** (H1–H3, M1–M5, L1–L8) with a `CLOSED/OPEN`
   column mapping every finding to the phase that resolved it:
   H1 ✓ Phase 0 (spec/README), H2 ✓ Phase 1.2, H3 ✓ Phase 4, M1 ✓ v2.1
   (documented deferral), M2 ✓ Phase 1.3 (delete-path; reintroduce under the
   settings-persist build), M3 ✓ Phase 1.5, M4 ✓ Phase 3.3 (framed NAK),
   M5 ✓ Phase 4 (event ring + EVT_OVERFLOW), L-items noted inline.
3. Tag `protocol-v2-firmware` (device) and `protocol-v2-app` (Java).
4. Ship note: units in the field (trackers) must be **reflashed**: a v1
   tracker and a v2 GS are *incompatible on air* (v2 GS will not hear a v1
   tracker). Include the bench procedure (Phases 0–4) as the flash script for
   multi-unit deployments — one GS unit at a time, all its trackers after.

---

## Appendix — Risk ledger

| Risk | Mitigation |
|---|---|
| Flash gate misses mid-Phase-2/3 | Emergency reserves: raw ADC (−1.0 KB), raw UART (−1.0 KB), DBG ASCII (−300 B), EVT_LORA_RX (−30 B) |
| TinyUSB buffer race never seen in v1 (print ignored errors) | Phase 2 framer unit vectors + hardware hex-dump transcripts in review |
| v1 BT behaviour unknowingly depended upon from Java (`lastRawRemote` hack) | Phase 6.3 UI capture comparison vs pre-migration app |
| Units in field: v1+v2 mixed fleets | Phase 7: cutover document; a v2 GS will not hear v1 trackers (v1 air parser deleted) |
| Settings persistence deferred (v2.1) | Explicit NOT_SUPPORTED wiring + caps bit 0 = 2 so hosts adapt |

## Appendix — file inventory (expected net diff)

| File | Action |
|---|---|
| `firmware/v2proto.c/.h` (new) | CRC, USB framer, dispatcher, events, subs |
| `firmware/v2air.c/.h` (new) | air codec + handlers |
| `firmware/Core/Src/usb.c` | gut ST stack, TX ring |
| `firmware/Core/Src/command.c`, `commands.c` | **delete** (Phase 2.0) |
| `firmware/Core/Src/gps.c` | drop NMEA unpack (Phase 2.0); `lastGpsSentence` refs (Phase 4) |
| `firmware/Core/Src/event_lora_rx.c` | → binary air handlers (Phase 3) |
| `firmware/Core/Src/filesystem.c`, `flash.c` | **delete** (Phase 1.3) |
| `firmware/Core/Src/tim.c` | **delete** (Phase 1.4) |
| `firmware/Core/Src/main.c` | boot-mode def (+settings restore v2.1) |
| `firmware/Core/Inc/tusb_config.h` | TX buffer 64→256 (Phase 1.2) |
| `firmware/Core/Inc/stm32f0xx_hal_conf.h` | drop HAL_PCD/ADC per phase |
| `firmware/Makefile` | source list per phase |
| `SimpleTrackerJava core` | Phase 6 as listed |
| `tools/v2bench.py` (new) | Phase 0 |
| `tools/bench/*` (new) | transcripts + rollback binaries |
