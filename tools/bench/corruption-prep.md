# Next session: tracking down the 1-byte-per-burst corruption

## Symptom (established 2026-11-07)
Every received burst carries 0-2 corrupted bytes; many bursts exactly ONE byte
at a ~fixed payload-position (≈bytes 18-21 of the frame, i.e. the middle of the
payload), with a bit pattern that VARIES per burst but lands in the same area:

  '2'(0x32) -> 0x92  (bit 7 set)      '1'(0x31) -> '0'(0x30)  (bit 0 cleared)
  '3'(0x33) -> 0x37  (bit 2 set)      'A'(0x41) -> '!'(0x21)  (bit 5 cleared)
  '0'(0x30) -> 0x10  (bit 5 cleared)

- Stable when re-read from memory (4x identical R readbacks) -> NOT a CDC/read
  glitch; the corrupted bytes are what the firmware stored.
- RSSI at the time was -25 (close) and -47..-54 (separated): corruption does
  NOT track RSSI/signal strength.
- Author intent: crcEnabled=0 is deliberate (weak-packet recovery design goal),
  v2 air plane will add app-level CRC16 + corruption flag.

## What it is NOT
- NOT thermal XOSC drift: author reports SX1262 crystal drift garbles the
  back half of long transmissions; our pattern is 1-2 bytes mid-payload,
  occurs on the FIRST burst after long idle, and re-displays identically.
- NOT CD C read-side corruption (stable re-reads), NOT USB truncation (H2 is
  a truncation bug, not bit-flips).

## Why it is strange
CR_4_5 LoRa FEC corrects single-symbol errors; surviving single-bit flips that
reproduced deterministically per burst position are not normal air noise.
Also RX failures stack: no HW CRC (by design) so corrupt frames pass to the
app parsers unchecked, where (1) corrupted ACK payloads fail the
strncmp("ACK") -> the GS silently flips to TRACKER runtime mode (kills all
further ACK handling until reboot) and (2) lastLoraMessage buffer gets
overwritten by the next arriving frame (buffer-swap races).

## Next-session experiment plan (highest discrimination first)
1. XXX-DUAL-RECEIVER TEST: broadcast one burst addressed to ffffffff, read
   BOTH units' lastLoraMessage. Corrupt on one, clean on the other =>
   per-unit radio problem; identical corruption => TX-side or RX-common.
2. TX-side elimination: A transmits fixed payload N times; B re-transmits the
   EXACT string it just received back to A; compare A's original vs B's echo.
   Corrupt echo => radio chain (TX or RF); clean echo => A's RX-side read.
3. Position-shift test: prepend 4, 8, 16 bytes of 'A's to the payload; does
   the corrupted byte position shift with content, or stay at absolute byte
   ~19-20 of the frame? Absolute => fixed-offset corruption (buffer pointer /
   LoRa_RX_buffer offset / symbol boundaries); content-shifted => encoding.
4. LoRa_Tick-layer probe: re-add a minimal trace hook to log LoRa_ReadPacket's
   GetRxBufferStatus payloadLen + startPtr + IRQ status per burst; compare
   against the stored len. A len/pointer mismatch is a smoking gun for the
   RX-buffer-offset class.
5. SNR per burst: LoRaRxInfo_t carries SNR; log it alongside RSSI on each
   received frame. Corrupt-but-high-SNR frames => NOT air noise => TX/build.
6. If 1-3 point at SX1262 RX-buffer offsets: check SetBufferBaseAddress /
   LoRa_SetBufferOffsets usage in LoRa_Receive/ReadPacket and whether the
   RX pointer is reset per burst (or wraps between bursts when unread).
7. HW CRC toggling experiment: temporarily set crcEnabled=1 on BOTH units and
   see which bursts get dropped at the radio (documents the corruption rate
   at the radio layer: radio-level drops vs app-level pass-through).

## Bench state at wrap-up
- Firmware: TEMP-DEBUG stripped, clean build (text 30,952, bin 31,344 B,
  static RAM 4,328 B / 792 unreserved after 1,024 stack reserve).
- Buffered fixes carried: STANDBY-before-TX in LoRa_Transmit + SetTxParams
  (authored txPower setting) in LoRa_ApplyConfig.
- The flashed units still carry the diagnostic (probe) build; flash the clean
  build next session (2 DFU rounds: A d0f85e68, B ecceb7e8 - hub path works).
- Phase 0.2 definitive baseline capture still pending on the clean build.
