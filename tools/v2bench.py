#!/usr/bin/env python3
"""v2bench.py — SimpleTracker V2 protocol reference codec and bench harness.

Reference implementation (host side) of docs/COMMUNICATION_SPEC_V2.md:

  * crc16 / encode / decode of USB frames        (spec §3)
  * air-frame encode / decode                    (spec §7.4)
  * GpsPacket_t decode                           (spec §10)
  * --self     : reproduce Appendix A wire vectors, byte-for-byte
  * --port ... : interactive console against a real unit (v2 mode)
  * --v1smoke  : scripted smoke test of the *v1* ASCII protocol
                 (needed for migration Phases 0-1, before the firmware
                 speaks v2)

Only dependency for hardware use: pyserial (`pip install pyserial`).

Exit codes: 0 = pass / ok, 1 = fail.
"""

import argparse
import binascii
import os
import sys
import time

SYNC = b"\xa5\x5a"
VER = 0x02
MAX_PAYLOAD = 128

TYPE_CMD = 0x01
TYPE_RSP_OK = 0x02
TYPE_RSP_ERR = 0x03
TYPE_EVT = 0x04
TYPE_DBG = 0x05

TYPE_NAMES = {1: "CMD", 2: "RSP_OK", 3: "RSP_ERR", 4: "EVT", 5: "DBG"}

OPCODES = {
    "HELLO": 0x00, "INFO": 0x01, "NOP2": 0x02, "RADIO_CONFIG": 0x03,
    "RADIO_READ": 0x04, "DISCOVER": 0x05, "DISCOVER_CANCEL": 0x06,
    "PAIR": 0x07, "OP8": 0x08, "SUBSCRIBE": 0x09, "UNSUBSCRIBE": 0x0A,
    "SUBS_READ": 0x0B, "SEND_AIR": 0x0C, "VOLTAGE_REQ": 0x0D,
    "SETTINGS_GET": 0x0E, "SETTINGS_SET": 0x0F, "SETTINGS_SAVE": 0x10,
    "SETTINGS_RELOAD": 0x11, "REBOOT": 0x12, "FACTORY": 0x13,
    "DEBUG_MODE": 0x14,
}
OPCODE_NAMES = {v: k for k, v in OPCODES.items()}

EVENTS = {
    "EVT_DISCOVERED": 1, "EVT_DISCOVERY_DONE": 2, "EVT_PAIR_STATUS": 3,
    "EVT_TELEMETRY": 4, "EVT_VOLTAGE": 5, "EVT_LORA_RX": 6,
    "EVT_OFFLINE": 7, "EVT_OVERFLOW": 8, "EVT_LOCAL_FIX": 9,
}
EVENT_NAMES = {v: k for k, v in EVENTS.items()}

ERR_CODES = {
    1: "BAD_LEN", 2: "BAD_OPCODE", 3: "BAD_ARG", 4: "BUSY",
    5: "NOT_SUPPORTED", 6: "RADIO_BUSY", 7: "NO_SUB", 8: "BLOCKED",
}

# Air message types, spec §7.5
AIR_TYPES = {
    "INQ_REQ": 0x01, "INQ_REP": 0x02, "PAIR_CMD": 0x03, "PAIR_ACK": 0x04,
    "VOLT_REQ": 0x05, "VOLT_REP": 0x06, "NAK": 0x07, "TELEMETRY": 0x20,
}
AIR_TYPE_NAMES = {v: k for k, v in AIR_TYPES.items()}

AIR_BROADCAST = 0xFFFFFFFF


# ---------------------------------------------------------------------------
# CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final xor)
# ---------------------------------------------------------------------------

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


# ---------------------------------------------------------------------------
# USB frame codec, spec §3
# ---------------------------------------------------------------------------

def encode_frame(ctype: int, seq: int, payload: bytes) -> bytes:
    """Build one USB frame. CRC covers VER..payload end (bytes 2..8+LEN-1)."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload {len(payload)} B > MAX_PAYLOAD {MAX_PAYLOAD}")
    body = bytes([
        VER, ctype & 0xFF,
        seq & 0xFF, (seq >> 8) & 0xFF,
        len(payload) & 0xFF, (len(payload) >> 8) & 0xFF,
    ]) + payload
    c = crc16(body)
    return SYNC + body + bytes([c & 0xFF, (c >> 8) & 0xFF])


class DecodeError(Exception):
    pass


class Decoder:
    """Streaming USB-frame decoder. feed() any number of bytes at any time;
    yields decoded (type, seq, payload) tuples. Resync scans forward for
    A5 5A 02 on any framing/CRC error."""

    def __init__(self):
        self.buf = bytearray()
        self.drop_count = 0

    def feed(self, data) -> list:
        self.buf.extend(data)
        out = []
        while True:
            f = self._try_frame(out)
            if not f:
                break
        return out

    def _try_frame(self, out) -> bool:
        buf = self.buf
        # scan for sync + VER==2
        idx = buf.find(SYNC)
        if idx < 0:
            del buf[:-1]
            return False
        if idx > 0:
            del buf[:idx]
        if len(buf) < 8:
            return False
        ln = buf[6] | (buf[7] << 8)
        if ln > MAX_PAYLOAD:
            self._drop("LEN")
            return True
        need = 8 + ln + 2
        if len(buf) < need:
            return False
        body = bytes(buf[2:need - 2])
        want = buf[need - 2] | (buf[need - 1] << 8)
        if crc16(body) != want:
            self._drop("CRC")
            return True
        ctype, seq, payload = buf[3], buf[4] | (buf[5] << 8), bytes(buf[8:need - 2])
        del buf[:need]
        out.append((ctype, seq, payload))
        return True

    def _drop(self, why):
        self.drop_count += 1
        self.buf = self.buf[1:]  # resync one byte at a time


def hexs(b: bytes, sep: str = " ") -> str:
    return sep.join(f"{x:02X}" for x in b)


# ---------------------------------------------------------------------------
# Air frame codec, spec §7.4/§7.5 (LoRa packet CRC is separate; not modelled)
# ---------------------------------------------------------------------------

def encode_air(src: int, dst: int, air_type: int, payload: bytes = b"") -> bytes:
    full = 0 if (air_type >= 0x20 and dst == AIR_BROADCAST) else 1
    # compact form (src only) is only legal tracker->GS, type >= 0x20, on a
    # dedicated channel; the host always builds full frames.
    if not (0 <= len(payload) <= 24):
        raise ValueError("air payload must be 0..24 bytes")
    if full:
        return (src.to_bytes(4, "little") + dst.to_bytes(4, "little")
                + bytes([air_type, len(payload)]) + payload)
    return src.to_bytes(4, "little") + bytes([air_type, len(payload)]) + payload


def decode_air(frame: bytes):
    """Returns dict(src, dst, type, payload). dst=None for compact frames."""
    if len(frame) < 10:
        raise DecodeError(f"air frame too short ({len(frame)} B)")
    src = int.from_bytes(frame[0:4], "little")
    ctype = frame[4]
    ln = frame[5]
    rest = frame[6:]
    if ctype >= 0x20:  # compact or full — disambiguate by length
        if 6 + ln == len(frame):
            dst, payload, off = None, rest[:ln], None
        elif 10 + ln == len(frame):
            dst = int.from_bytes(frame[4:8], "little")
            ctype, ln = frame[8], frame[9]
            payload = frame[10:10 + ln]
        else:
            raise DecodeError("length/type mismatch")
    else:
        dst = int.from_bytes(frame[4:8], "little")
        ln = frame[8]
        payload = frame[9:9 + ln]
    return {"src": src, "dst": dst, "type": ctype, "payload": payload}


# ---------------------------------------------------------------------------
# GpsPacket_t decode, spec §10 (19 bytes, little-endian, == GPS_BINARY_PACKET.md)
# ---------------------------------------------------------------------------

def decode_gps_packet(pkt: bytes) -> dict:
    if len(pkt) != 19:
        raise DecodeError(f"GpsPacket_t is 19 bytes, got {len(pkt)}")
    def s32(v):
        return v - (1 << 32) if v >= (1 << 31) else v
    time_s = int.from_bytes(pkt[0:4], "little")
    lat = s32(int.from_bytes(pkt[4:8], "little"))
    lon = s32(int.from_bytes(pkt[8:12], "little"))
    alt = int.from_bytes(pkt[12:14], "little", signed=True)
    geoid = int.from_bytes(pkt[14:16], "little", signed=True)
    return {
        "time_s": time_s, "lat_raw": lat, "lon_raw": lon, "alt_m": alt,
        "geoid_dm": geoid, "sats": pkt[16], "hdop_x10": pkt[17],
        "flags": pkt[18], "fix": pkt[18] & 0x07,
    }


def gps_packet_summary(d: dict) -> str:
    def ddmm_mmm(raw):
        """GpsPacket_t lat/lon raw = ddmm.mmmmm × 100 000, sign = hemisphere."""
        sign = "-" if raw < 0 else ""
        m_total = abs(raw) / 100000.0           # ddmm.mmmmm as a number
        degrees = int(m_total // 100)
        minutes = m_total - degrees * 100
        return f"{sign}{degrees:02d}°{minutes:07.4f}'"
    lat = ddmm_mmm(d["lat_raw"])
    lon = ddmm_mmm(d["lon_raw"])
    hh = (d["time_s"] // 3600) % 24
    mm = (d["time_s"] % 3600) // 60
    ss = d["time_s"] % 60
    return (f"{hh:02d}:{mm:02d}:{ss:02d} UTC "
            f"lat={lat} lon={lon} alt={d['alt_m']}m "
            f"geoid={d['geoid_dm'] / 10:.1f}m sats={d['sats']} "
            f"hdop={d['hdop_x10'] / 10:.1f} fix={d['fix']}")


# ---------------------------------------------------------------------------
# Payload builders (commands) and pretty renderers
# ---------------------------------------------------------------------------

def build_cmd(name: str, args: list) -> bytes:
    """`cmd NAME arg...` convenience payload, spec §4."""
    op = OPCODES[name]
    args = list(args)
    if name == "HELLO" or name == "INFO" or name in ("RADIO_READ", "SUBS_READ",
                                                     "DISCOVER_CANCEL", "SETTINGS_GET",
                                                     "REBOOT", "FACTORY"):
        return bytes([op])  # trailing optional args ignored safely
    if name == "RADIO_CONFIG":
        freq, sf, bw, cr, pwr = args
        return bytes([op]) + int(freq).to_bytes(4, "little") + bytes(
            [int(sf), int(bw), int(cr), int(pwr)])
    if name == "DISCOVER":
        window, mx = args if args else (6000, 10)
        return bytes([op]) + int(window).to_bytes(2, "little") + bytes([int(mx)])
    if name == "PAIR":
        uid, freq, sf, bw = args
        return bytes([op]) + parse_uid(uid) + int(freq).to_bytes(4, "little") \
            + bytes([int(sf), int(bw)])
    if name in ("SUBSCRIBE",):
        uid, flags = args
        return bytes([op]) + parse_uid(uid) + bytes([int(flags)])
    if name == "UNSUBSCRIBE":
        return bytes([op]) + parse_uid(args[0])
    if name == "VOLTAGE_REQ":
        return bytes([op]) + parse_uid(args[0])
    if name == "SETTINGS_SET":
        label, value = args
        if len(label) != 1:
            raise ValueError("setting label is 1 ASCII char")
        return bytes([op]) + label.encode() + int(value).to_bytes(4, "little")
    if name == "DEBUG_MODE":
        return bytes([op]) + bytes([int(args[0])]) if args else bytes([op])
    if name == "SEND_AIR":
        dst, atype, *pl = args
        payload = bytes(parse_hex_list(" ".join(pl))) if pl else b""
        if isinstance(atype, str):
            atype = AIR_TYPES.get(atype.upper(), int(atype, 0))
        return bytes([op]) + parse_uid(dst) + bytes([atype]) + payload
    raise ValueError(f"no builder for {name}")


def parse_uid(s: str) -> bytes:
    s = s.strip().lower()
    try:
        return int(s, 16).to_bytes(4, "little")
    except ValueError:
        pass
    # 8-char hex string a1b2c3d4 → little-endian bytes d4 c3 b2 a1
    if len(s) == 8:
        try:
            return int(s, 16).to_bytes(4, "little")
        except ValueError:
            return s.encode()[:4].ljust(4, b"\x00")
    raise ValueError(f"cannot parse UID {s!r}")


def parse_hex_list(s: str) -> bytes:
    return binascii.unhexlify(s.replace(" ", "").replace(",", ""))


def render_evt(payload: bytes) -> str:
    if len(payload) < 1:
        return "?"
    ev = payload[0]
    name = EVENT_NAMES.get(ev, f"EVT_{ev:02X}")
    rest = payload[1:]
    try:
        if name == "EVT_DISCOVERED":
            uid = int.from_bytes(rest[0:4], "little")
            rssi = int.from_bytes(rest[4:5], "little", signed=True)
            return f"{name} uid={uid:08x} rssi={rssi} ({len(rest)} B)"
        if name == "EVT_DISCOVERY_DONE":
            return f"{name} count={rest[0]}"
        if name == "EVT_PAIR_STATUS":
            uid = int.from_bytes(rest[0:4], "little")
            status = {0: "ACKED", 1: "TIMEOUT", 2: "REJECTED"}.get(rest[4], rest[4])
            rssi = int.from_bytes(rest[5:6], "little", signed=True)
            return f"{name} uid={uid:08x} status={status} rssi={rssi}"
        if name == "EVT_TELEMETRY":
            uid = int.from_bytes(rest[0:4], "little")
            rssi = int.from_bytes(rest[4:5], "little", signed=True)
            pkt = rest[5:24]
            return f"{name} uid={uid:08x} rssi={rssi} {gps_packet_summary(decode_gps_packet(pkt))}"
        if name == "EVT_VOLTAGE":
            uid = int.from_bytes(rest[0:4], "little")
            mv = int.from_bytes(rest[4:6], "little")
            rssi = int.from_bytes(rest[6:7], "little", signed=True)
            return f"{name} uid={uid:08x} mv={mv} rssi={rssi}"
        if name == "EVT_LORA_RX":
            uid = int.from_bytes(rest[0:4], "little")
            rssi = int.from_bytes(rest[4:5], "little", signed=True)
            ln = rest[5]
            return f"{name} uid={uid:08x} rssi={rssi} len={ln} payload={hexs(rest[6:6 + ln])}"
        if name == "EVT_OFFLINE":
            uid = int.from_bytes(rest[0:4], "little")
            age = int.from_bytes(rest[4:6], "little")
            return f"{name} uid={uid:08x} age_s={age}"
        if name == "EVT_OVERFLOW":
            dropped = int.from_bytes(rest[0:2], "little")
            last_seq = int.from_bytes(rest[2:4], "little")
            return f"{name} dropped={dropped} last_evt_seq={last_seq}"
        if name == "EVT_LOCAL_FIX":
            return f"{name} {gps_packet_summary(decode_gps_packet(rest[:19]))}"
    except Exception as e:
        return f"{name} (decode error: {e}) raw={hexs(rest)}"
    return f"{name} raw={hexs(rest)}"


def render_frame(ctype, seq, payload, verbose=False) -> str:
    tname = TYPE_NAMES.get(ctype, f"TYPE{ctype:02X}")
    if ctype == TYPE_RSP_OK:
        if payload:
            return f"← RSP_OK SEQ{seq} {hexs(payload)}"
        return f"← RSP_OK SEQ{seq}"
    if ctype == TYPE_RSP_ERR:
        code = payload[0] if payload else -1
        detail = payload[1:].decode("ascii", "replace").strip("\x00\n") if len(payload) > 1 else ""
        name = ERR_CODES.get(code, f"ERR{code}")
        return f"← RSP_ERR SEQ{seq} {name}({code}) {detail}".rstrip()
    if ctype == TYPE_EVT:
        return f"← {render_evt(payload)}"
    if ctype == TYPE_DBG:
        return f"← DBG | {payload.decode('ascii', 'replace').rstrip()}"
    if ctype == TYPE_CMD:
        op = payload[0] if payload else 0xFF
        return f"→ CMD SEQ{seq} {OPCODE_NAMES.get(op, hex(op))} {hexs(payload[1:])}"
    return f"¿ {tname} SEQ{seq} {hexs(payload)}"


# ---------------------------------------------------------------------------
# Appendix A vectors (docs/COMMUNICATION_SPEC_V2.md)
# ---------------------------------------------------------------------------

VECTORS = {
    "1 HELLO cmd SEQ=1": (
        encode_frame(TYPE_CMD, 1, bytes([0x00])),
        "A5 5A 02 01 01 00 01 00 00 EC 49"),
    "2 HELLO rsp SEQ=1": (
        encode_frame(TYPE_RSP_OK, 1,
                     bytes.fromhex("02000F00") + int.to_bytes(0x1A2B3C4D, 4, "little")),
        "A5 5A 02 02 01 00 08 00 02 00 0F 00 4D 3C 2B 1A 4C 58"),
    "3 INFO cmd SEQ=2": (
        encode_frame(TYPE_CMD, 2, bytes([0x01])),
        "A5 5A 02 01 02 00 01 00 01 1F B7"),
    "4 DISCOVER cmd SEQ=3": (
        encode_frame(TYPE_CMD, 3, bytes([0x05, 0x70, 0x17, 0x0A])),
        "A5 5A 02 01 03 00 04 00 05 70 17 0A A7 45"),
    "5 SUBSCRIBE cmd SEQ=4": (
        encode_frame(TYPE_CMD, 4, bytes([0x09]) + bytes.fromhex("D4C3B2A1") + bytes([0x01])),
        "A5 5A 02 01 04 00 06 00 09 D4 C3 B2 A1 01 2B 07"),
    "6 EVT_DISCOVERED eSEQ=4": (
        encode_frame(TYPE_EVT, 4, bytes([0x01]) + bytes.fromhex("D4C3B2A1") + b"\xb8"),
        "A5 5A 02 04 04 00 06 00 01 D4 C3 B2 A1 B8 95 B5"),
    "7 EVT_TELEMETRY eSEQ=5 (zero pkt)": (
        encode_frame(TYPE_EVT, 5,
                     bytes([0x04]) + bytes.fromhex("D4C3B2A1") + b"\xb8" + bytes(19)),
        "A5 5A 02 04 05 00 19 00 04 D4 C3 B2 A1 B8 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 B8 75"),
    "8 RSP_ERR SEQ=9 BAD_ARG": (
        encode_frame(TYPE_RSP_ERR, 9, bytes([0x03])),
        "A5 5A 02 03 09 00 01 00 03 E2 F0"),
    "air PAIR_CMD (full, 15 B)": (
        encode_air(0x1A2B3C4D, 0xA1B2C3D4, AIR_TYPES["PAIR_CMD"],
                   int(434500000).to_bytes(4, "little") + bytes([9, 3])),
        "4D 3C 2B 1A D4 C3 B2 A1 03 06 A0 F1 E5 19 09 03"),
}


def self_test() -> bool:
    ok = True
    for name, (got, want) in VECTORS.items():
        want_b = bytes.fromhex(want.replace(" ", "")) if name.startswith("air") \
            else bytes.fromhex(want)
        match = got == want_b
        print(f"{'PASS' if match else 'FAIL'}  {name:36s}  {hexs(got)}")
        if not match:
            ok = False
            print(f"       expected: {want}")

    # decoder round-trips every USB vector, incl. noise prefix and split feeds
    usb_blob = b"".join(g for g, _ in VECTORS.values() if g[0] == 0xA5)
    n_usb = sum(1 for g, _ in VECTORS.values() if g[0] == 0xA5)

    dec = Decoder()
    frames = dec.feed(b"\x00\xFF\x55" + usb_blob)
    print(f"{'PASS' if len(frames) == n_usb else 'FAIL'}  "
          f"decoder resync round-trip: {len(frames)}/{n_usb} frames from noisy stream")

    dec3 = Decoder()
    acc = []
    for i in range(len(usb_blob)):
        acc.extend(dec3.feed(usb_blob[i:i + 1]))
    print(f"{'PASS' if len(acc) == n_usb else 'FAIL'}  "
          f"byte-at-a-time feed: {len(acc)}/{n_usb} frames")
    frames_ok = len(frames) == n_usb and len(acc) == n_usb

    # decoder rejects a corrupted CRC frame without disturbing the next one
    dec4 = Decoder()
    good = encode_frame(TYPE_RSP_OK, 42, b"")
    corrupt = bytearray(good)
    corrupt[-1] ^= 0xFF
    frames4 = dec4.feed(bytes(corrupt) + good)
    rej = (len(frames4) == 1 and dec4.drop_count == 1)
    print(f"{'PASS' if rej else 'FAIL'}  "
          f"bad CRC dropped (drop_count={dec4.drop_count}), good frame intact")

    return ok and frames_ok and rej


# ---------------------------------------------------------------------------
# Serial helpers
# ---------------------------------------------------------------------------

def open_port(port, baud=115200, timeout=0.5):
    import serial
    return serial.serial_for_url(port, baud, timeout=timeout,
                                 write_timeout=1.0)


def drain_line(ser, timeout=5.0):
    """Read until end-of-line or timeout. Returns str without terminator."""
    import time as _t
    deadline = _t.time() + timeout
    out = bytearray()
    while _t.time() < deadline:
        chunk = ser.read(64)
        if chunk:
            out.extend(chunk)
            if out[-1:] in (b"\n", b"\r"):
                return out.decode("ascii", "replace").rstrip("\r\n")
        else:
            if out:
                return out.decode("ascii", "replace").rstrip("\r\n")
    if out:
        return out.decode("ascii", "replace").rstrip("\r\n")
    return None


def wait_for_dtr(ser, want=True, timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if bool(ser.dtr) == want:
            return True
        time.sleep(0.05)
    return bool(ser.dtr) == want


# ---------------------------------------------------------------------------
# v1 smoke test (ASCII protocol, pre-V2 firmware)
# ---------------------------------------------------------------------------

def v1smoke(ser, scan=True, out=None):
    """Run the Phase-0 v1 smoke. Unit A must be the GS unit on the bench;
    unit B (tracker boot) is optional and only needed for SCAN/PAIR checks."""
    log = out.write if out else (lambda *a: print(*a))

    def step(cmd, expect_timeout=2.0, comment=""):
        if comment:
            log(f"# {comment}")
        t0 = time.time()
        ser.reset_input_buffer()
        ser.write(cmd.encode() + b"\n")
        resp = drain_line(ser, expect_timeout)
        dt = (time.time() - t0) * 1000
        log(f">>> {cmd!r}")
        log(f"<<< {resp!r}  ({dt:.0f} ms)")
        return resp

    results = []

    def check(label, cond):
        results.append((label, bool(cond)))
        log(("PASS  " if cond else "FAIL  ") + label)

    step("i", 2.0, "disable interactive mode in case a previous session left it on")
    r = step("UID")
    check("UID: 8 hex chars", r is not None and len(r.strip()) == 8
          and all(c in "0123456789abcdefABCDEF" for c in r.strip()))

    r = step("GET b")
    check("GET b == 3 (62.5 kHz default, H1)", r == "3")

    r = step("GET f")
    check("GET f == 434000000", r == "434000000")

    r = step("SET b 2")
    check("SET b 2 -> OK", r == "OK")
    r = step("GET b")
    check("GET b == 2 after SET", r == "2")
    r = step("SET b 3")
    check("SET b 3 (restore) -> OK", r == "OK")
    r = step("GET b")
    check("GET b == 3 restored", r == "3")

    r = step("SET zz 1")
    check("SET zz invalid label -> ERR", r == "ERR")

    if scan:
        log("# --- SCAN + D (needs unit B powered, tracker boot) ---")
        r = step("SCAN")
        check("SCAN -> OK", r == "OK")
        time.sleep(1.0)
        found = False
        for i in range(10):
            ser.write(b"D\n")
            r = drain_line(ser, 6.5)
            means = "in-progress" if (r or "").startswith("WAIT") else "final"
            log(f"<<< (D poll {i + 1}, {means}) {r!r}")
            if r is None:
                break
            if r.startswith("WAIT"):
                time.sleep(1.0)
                continue
            # a completed scan answers "<count>[ uid,rssi ...]" (v1 §2.3);
            # NONE means no scan was registered or the window isn't done
            found = r[0] in "0123456789" and ("," in r or r.strip() == "0")
            break
        r2 = step("D", 3.0, "re-confirm after the 6 s window")
        if not found and r2 and r2[0] in "0123456789":
            found = True
        check("SCAN -> D reaches a completed result set (count line)", found)

    r = step("R", 2.0, "read last LoRa message (may be empty, H2 >64B check)")
    log(f"# R response length: {len(r) if r else 0}")
    if r and r.startswith("$"):
        check("R: NMEA sentence + rssi (len=" + str(len(r)) + ")", len(r) > 80)

    log("")
    fails = [n for n, okres in results if not okres]
    log(f"v1smoke: {len(results) - len(fails)}/{len(results)} checks passed"
        + (f", FAILED: {', '.join(fails)}" if fails else ""))
    return not fails


# ---------------------------------------------------------------------------
# Interactive v2 console
# ---------------------------------------------------------------------------

class Console:
    def __init__(self, ser, path):
        self.ser = ser
        self.path = path
        self.seq = 0
        self.dec = Decoder()
        self.logf = None
        self.pending = {}

    def log(self, msg):
        stamp = time.time()
        print(msg, flush=True)
        if self.logf:
            self.logf.write(f"{stamp:.3f} {msg}\n")
            self.logf.flush()

    def pump(self):
        n = self.ser.in_waiting
        if n:
            for f in self.dec.feed(self.ser.read(n)):
                self.log("   " + render_frame(f[0], f[1], f[2]))

    def next_seq(self):
        self.seq += 1
        return self.seq

    def send_cmd(self, name, args):
        payload = build_cmd(name, args)
        fr = encode_frame(TYPE_CMD, self.next_seq(), payload)
        self.log(f">>> {name} {args!r}")
        self.ser.write(fr)

    def loop(self):
        import threading
        stop = threading.Event()

        def bg():
            while not stop.is_set():
                try:
                    self.pump()
                except Exception as e:
                    self.log(f"!! pump: {e}")
                    return
                time.sleep(0.02)

        th = threading.Thread(target=bg, daemon=True)
        th.start()
        print(f"v2bench console on {self.path}. Commands:")
        print("  cmd <OPCODE> [args...]      send a CMD frame (e.g. cmd HELLO)")
        print("  hex A5 5A ..                send raw bytes")
        print("  expect RSP_OK|RSP_ERR       tag next response")
        print("  logfile <path>              also save transcript")
        print("  quit")
        try:
            while True:
                line = input("v2> ").strip()
                if not line or line.startswith("#"):
                    continue
                if line in ("quit", "exit", "q"):
                    break
                try:
                    self.exec_line(line)
                except Exception as e:
                    print(f"!! {e}")
        except (EOFError, KeyboardInterrupt):
            pass
        finally:
            stop.set()

    def exec_line(self, line):
        parts = line.split()
        if parts[0] == "hex":
            data = parse_hex_list(" ".join(parts[1:]))
            self.ser.write(data)
            return
        if parts[0] == "logfile":
            self.logf = open(parts[1], "a")
            print(f"(logging to {parts[1]})")
            return
        if parts[0] == "cmd":
            name = parts[1].upper()
            if name not in OPCODES:
                name = {v: k for k, v in OPCODES.items()}.get(int(parts[1], 0))
            args = parts[2:]
            self.send_cmd(name, args)
            return
        print("?? unknown; use cmd/hex/logfile/quit")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--self", action="store_true",
                    help="run CRC + wire-vector self test, no hardware")
    ap.add_argument("--port", help="serial port of the GS unit (v2 mode)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--v1smoke", action="store_true",
                    help="run the v1 ASCII smoke test")
    ap.add_argument("--no-scan", action="store_true",
                    help="v1smoke: skip SCAN/PAIR checks (unit A only)")
    ap.add_argument("--transcript", help="append output to this file")
    ap.add_argument("--dtr", type=int, default=None,
                    help="assert/deassert DTR explicitly (0/1)")
    args = ap.parse_args()

    if args.self:
        ok = self_test()
        sys.exit(0 if ok else 1)

    if not args.port:
        ap.error("--port is required for --interactive/--v1smoke "
                 "(use --self for the hardware-free self test)")

    kwargs = {}
    if args.transcript:
        os.makedirs(os.path.dirname(os.path.abspath(args.transcript)) or ".", exist_ok=True)
        kwargs["out"] = open(args.transcript, "a", encoding="utf-8")
        kwargs["out"].write(f"\n===== {time.ctime()} "
                            f"{'v1smoke' if args.v1smoke else 'console'} on {args.port} =====\n")

    ser = open_port(args.port, args.baud)
    if args.dtr is not None:
        ser.dtr = bool(args.dtr)
    time.sleep(0.5)  # let the CDC open settle

    try:
        if args.v1smoke:
            ok = v1smoke(ser, scan=not args.no_scan, **kwargs)
            sys.exit(0 if ok else 1)
        console = Console(ser, args.port)
        if kwargs.get("out"):
            console.logf = kwargs["out"]
            print(f"(transcript: {args.transcript})")
        console.loop()
    finally:
        ser.close()
        if kwargs.get("out"):
            kwargs["out"].close()


if __name__ == "__main__":
    main()
