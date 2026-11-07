#!/usr/bin/env python3
"""flash.py — automated bench flashing for the SimpleTracker STM32F042 units.

Full flash/run cycle with zero manual GPIO operations once the bench
bootloader is installed on a unit (one-time install needs the ROM DFU =
BOOT0 pin shorted + power cycle, per the phase plan):

    ./flash.py build_ltapp/firmware_ltapp.bin            # flash + verify
    ./flash.py --install-bl build_bl/bootloader.bin      # one-time BL install

Sequence: identify unit over CDC -> "REBOOT BL" -> bootloader re-enumerates
as "SimpleTracker BL" (PID 0x4001, same UID serial) -> binary frame protocol
(CRC16-CCITT-FALSE, same CRC as the V2 comm spec) -> ERASE / WRITE / FINALIZE
/ GO -> app re-enumerates -> VER handshake.

The bootloader's torn-image safety lives on the device side: info-page magic
+ whole-image CRC are re-verified on every boot, so an interrupted flash
boots back into update mode and can be redone from the same USB cable.
"""

import argparse
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("pyserial required:  sudo apt install python3-serial   (or pip3 install pyserial)")

# Bootloader frame protocol (see bootloader/bl_main.c)
SOF = b"\xC6\x6C"
FR_INFO, FR_ERASE, FR_WRITE, FR_FINAL, FR_GO, FR_RESET = 0x10, 0x20, 0x30, 0x40, 0x50, 0x60
ACK = 0x80  # response = request | ACK
ERR_TYPE = 0xFE

# Layout mirrored from bootloader/boot_common.h
APP_BASE = 0x08002800
APP_END = 0x08007C00
APP_MAX = APP_END - APP_BASE          # 21504 (21 KB)
WRITE_CHUNK = 240                     # <=251 payload data, even, multiple of 16

VID = 0xCafe
PID_APP, PID_BL = 0x4000, 0x4001

# port discovery: pyserial comports() (VID/PID/serial straight from USB
# descriptors) — /dev/serial/by-id may not exist on all hosts


def _comports():
    for info in serial.tools.list_ports.comports():
        if info.vid is None:
            continue
        yield info


def app_ports():
    out = []
    for info in _comports():
        if info.vid == VID and info.pid == PID_APP:
            out.append(info)
    return out


def bl_ports():
    out = []
    for info in _comports():
        if info.vid == VID and info.pid == PID_BL:
            out.append(info)
    return out


def port_path(info):
    return info.device


def port_uid(info):
    return (info.serial_number or "").lower()


def port_desc(info):
    return f"{info.device} {info.pid:04x} {port_uid(info)} {getattr(info, 'description', '')}".strip()


def list_all():
    return list(_comports())


def wait_for(pred, timeout, what):
    """Poll comports until pred(info) is true; returns info or exits."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        for info in list_all():
            if pred(info):
                return info
        time.sleep(0.15)
    sys.exit(f"timeout: no {what} appeared within {timeout}s")


# ---------------------------------------------------------------------------
# CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF, MSB first, no reflection)

def crc16_calc(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
        crc &= 0xFFFF
    return crc


def crc16_selftest():
    if crc16_calc(b"123456789") != 0x29B1:
        raise SystemExit("crc16 selftest FAILED (expected CCITT-FALSE 0x29B1)")
    print("crc16 selftest OK")


# ---------------------------------------------------------------------------
# serial helpers

def open_port(path, timeout=1.0):
    return serial.Serial(path, baudrate=115200, timeout=timeout, write_timeout=2.0)


def drain(port):
    try:
        time.sleep(0.05)
        discarded = port.read(port.in_waiting or 1)
        return len(discarded) if discarded else 0
    except Exception:
        return 0


def frame_decode(port):
    """Read one response frame (SOF, type, len, payload, CRC16). Blocking."""
    def rd(n):
        data = port.read(n)
        if len(data) != n:
            sys.exit(f"frame read timeout (wanted {n}, got {len(data)} bytes)")
        return data

    # resync
    b = rd(1)
    tries = 0
    while b != SOF[:1]:
        tries += 1
        if tries > 512:
            sys.exit("no frame start found")
        b = rd(1)
    if rd(1) != SOF[1:]:
        sys.exit("bad SOF2")
    typ, ln = rd(2)
    payload = rd(ln)
    crc = int.from_bytes(rd(2), "little")
    calc = crc16_calc(bytes([typ, ln]) + payload)
    if calc != crc:
        sys.exit(f"frame crc mismatch: got {crc:04X} want {calc:04X}")
    return typ, payload


def send_frame(port, typ, payload=b""):
    frame = SOF + bytes([typ, len(payload)]) + payload
    frame += crc16_calc(bytes([typ, len(payload)]) + payload).to_bytes(2, "little")
    port.write(frame)
    port.flush()


class Bootloader:
    def __init__(self, port):
        self.ser = open_port(port)
        drain(self.ser)

    def info(self):
        send_frame(self.ser, FR_INFO)
        typ, p = frame_decode(self.ser)
        return typ, p

    # request/expect wrappers -------------------
    def step(self, typ, payload, want, timeout=2.0):
        self.ser.timeout = timeout
        send_frame(self.ser, typ, payload)
        rtyp, rp = frame_decode(self.ser)
        if rtyp == ERR_TYPE:
            code = rp[0] if rp else 0
            sys.exit(f"bootloader error {code} on request 0x{typ:02X} (payload {rp.hex() if rp else '-'})")
        if (rtyp & 0x7F) != typ:
            sys.exit(f"unexpected response type 0x{rtyp:02X} to 0x{typ:02X}")
        return rp

    def erase(self):
        return self.step(FR_ERASE, b"", want=None, timeout=4.0)

    def write(self, addr, data):
        payload = addr.to_bytes(4, "little") + data
        self.step(FR_WRITE, payload, want=None, timeout=2.0)

    def finalize(self, length, crc, build_id):
        payload = length.to_bytes(4, "little") + crc.to_bytes(2, "little")
        payload += build_id.to_bytes(4, "little")
        self.step(FR_FINAL, payload, want=None, timeout=3.0)

    def go(self):
        self.step(FR_GO, b"", want=None, timeout=2.0)

    def read(self, addr, n):
        # FR_READ: addr u32 + n u8; response payload = raw bytes
        self.ser.timeout = 2.0
        send_frame(self.ser, 0x70, addr.to_bytes(4, "little") + bytes([n]))
        rtyp, rp = frame_decode(self.ser)
        if rtyp == ERR_TYPE:
            sys.exit(f"read error at {addr:#x}: {rp.hex()}")
        if rtyp != (0x70 | ACK):
            sys.exit(f"unexpected response to READ: 0x{rtyp:02X}")
        return rp


# ---------------------------------------------------------------------------
# flashing flow

def send_line(port, line, expect=None, timeout=1.0):
    port.timeout = timeout
    port.reset_input_buffer()
    port.write(line.encode() + b"\n")
    port.flush()
    if expect is None:
        return ""
    resp = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        resp += port.read(port.in_waiting or 1)
        if expect.encode() in resp:
            return resp.decode(errors="replace").strip()
    return resp.decode(errors="replace").strip()


def flash_unit(image_path, uid=None, bl_mode=False):
    print(f"flash: {image_path}")
    img = open(image_path, "rb").read()
    if len(img) % 2:
        img += b"\xFF"          # pad to halfword; CRC includes padding (BL-identical)
    if len(img) > APP_MAX:
        sys.exit(f"image too large: {len(img)} > {APP_MAX} (app region {APP_BASE:#x}..{APP_END:#x})")

    port_app = None
    if not bl_mode:
        # 1. locate app port (fall back to BL mode if only a BL is present)
        sel = app_ports()
        if not sel and bl_ports():
            print("no app port but bootloader present -> continuing in BL mode")
            bl_mode = True
        elif not sel:
            sys.exit("no app CDC ports (PID 4000); is a unit plugged in?")
        else:
            if len(sel) > 1 and not uid:
                sys.exit(f"multiple app ports; disambiguate with --uid: {[port_desc(p) for p in sel]}")
            if uid:
                sel = [p for p in sel if uid.lower() in port_uid(p)]
                if not sel:
                    sys.exit(f"no app port matches --uid {uid}")
            port_app = sel[0]
            print(f"app port: {port_desc(port_app)}")

    if not bl_mode:
        # 2. banner + unit identity --------------------------------------------
        ser = open_port(port_path(port_app))
        drain(ser)
        ver = send_line(ser, "VER", expect="LT v")
        print(f"app VER: {ver!r}")
        # 3. reboot into BL ----------------------------------------------------
        rb = send_line(ser, "REBOOT BL", timeout=1.0)
        if "OK" not in rb:
            print(f"warning: REBOOT BL reply {rb!r} (reset may have cut the reply)")
        ser.close()

    # 4. bootloader port ---------------------------------------------------------
    if bl_mode:
        sel = [p for p in bl_ports() if not uid or uid.lower() in port_uid(p)]
        if not sel:
            sys.exit("--bl-mode but no bootloader port (PID 4001)")
        bl_info = sel[0]
    else:
        bl_info = wait_for(lambda p: p.pid == PID_BL, 10.0, "bootloader CDC")
    print(f"BL port: {port_desc(bl_info)}")
    bl_path = port_path(bl_info)
    time.sleep(0.3)
    bl = Bootloader(bl_path)
    time.sleep(0.3)
    ityp, inf = bl.info()     # INFO: handshake (also drains any stale frames)
    proto, uidc = inf[0], inf[4:12].decode()
    if proto != 1:
        sys.exit(f"bootloader protocol version {proto} != 1")
    app_valid = bool(inf[3] & 1)
    old_build = int.from_bytes(inf[12:16], "little")
    print(f"BL INFO: uid={uidc} proto={proto} app_valid={app_valid} build={old_build}")
    if uid and uidc != uid.lower():
        sys.exit(f"unit UID {uidc} does not match --uid {uid}")

    # 5. erase --------------------------------------------------------------------
    pages = int.from_bytes(bl.erase(), "little")
    print(f"ERASE: {pages} pages")

    # 6. write ----------------------------------------------------------------------
    crc = 0xFFFF
    sent = 0
    for off in range(0, len(img), WRITE_CHUNK):
        chunk = img[off:off + WRITE_CHUNK]
        bl.write(APP_BASE + sent, chunk)
        sent += len(chunk)
        if (sent // WRITE_CHUNK) % 40 == 0 or sent == len(img):
            print(f"  wrote {sent}/{len(img)}")
        crc = crc16_cont(crc, chunk)

    # 7. finalize ----------------------------------------------------------
    build = old_build + 1
    bl.finalize(sent, crc, build)
    print(f"FINALIZE: {sent}B crc={crc:04X} build={build}")

    # 7b. read-back verify: compare flash against the image we sent
    bad = 0
    for off in range(0, sent, 254):
        n = min(254, sent - off)
        got = bl.read(APP_BASE + off, n)
        if got != img[off:off+n]:
            print(f"MISMATCH at {APP_BASE+off:#x}: sent {img[off:off+n].hex()} got {got.hex()}")
            bad += 1
            if bad > 3:
                sys.exit("read-back verify failed (3+ chunks), image not as sent")
    if bad:
        sys.exit(f"read-back verify: {bad} mismatching chunk(s)")
    print("read-back verify OK")

    # 8. go -----------------------------------------------------------------
    bl.go()
    print("GO sent")
    bl.ser.close()

    # 9. verify app back -------------------------------------------------------
    old_uid = port_uid(bl_info)
    app_info = wait_for(lambda p: p.pid == PID_APP, 8.0, "app CDC")
    app_path = port_path(app_info)
    if port_app and app_path != port_path(port_app):
        print(f"note: app re-enumerated at {app_path} (port shuffle)")
    time.sleep(0.3)
    ok = False
    ver2 = ""
    for attempt in range(4):
        try:
            ser2 = open_port(app_path)
        except Exception as e:
            print(f"  app open retry {attempt}: {e}")
            time.sleep(1.0)
            continue
        drain(ser2)
        ver2 = send_line(ser2, "VER", expect="LT v", timeout=2.0)
        ser2.close()
        if "LT v" in ver2:
            ok = True
            break
        time.sleep(1.0)
    if not ok:
        sys.exit(f"app did not answer VER after flash: {ver2!r}")
    if port_uid(app_info) != old_uid:
        sys.exit(f"UID mismatch after flash: {port_uid(app_info)} != {old_uid}")
    print(f"verify OK: {ver2!r}")
    print("flash complete")


def crc16_cont(crc, data):
    """Continue a CCITT-FALSE CRC over new bytes (matches BL crc16_cont)."""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
        crc %= 0x10000
    return crc


def install_bl(port=None):
    """One-time: unit must be in ROM DFU (BOOT0 high at power-up)."""
    print("== one-time bootloader install ==")
    print("Requires the unit in ROM DFU mode: short BOOT0 to 3V/1, power cycle.")
    print("This is the LAST manual flash operation ever needed on this unit.")
    import subprocess
    subprocess.run(["dfu-util", "-a", "0", "-D", "build_bl/bootloader.bin", "-s", "0x08000000"], check=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", nargs="?", help="application .bin to flash")
    ap.add_argument("--uid", help="match by bootloader UID string (8 hex chars)")
    ap.add_argument("--install-bl", action="store_true", help="one-time BL install via dfu-util (ROM DFU)")
    ap.add_argument("--bl-mode", action="store_true", help="unit is already in bootloader update mode")
    ap.add_argument("--read", nargs=2, metavar=("ADDR", "N"), help="dump N bytes of flash from a BL-mode unit")
    ap.add_argument("--selftest", action="store_true", help="crc16 self-test only")
    args = ap.parse_args()

    if args.selftest:
        crc16_selftest()
        return
    if args.install_bl:
        install_bl()
        return
    crc16_selftest()
    if args.read:
        addr = int(args.read[0], 0)
        n = int(args.read[1], 0)
        bl_path = bl_ports()
        if not bl_path:
            sys.exit("no bootloader port for --read")
        bl = Bootloader(port_path(bl_path[0]))
        for off in range(0, n, 254):
            chunk = bl.read(addr + off, min(254, n - off))
            print(f"{addr+off:#010x}: {chunk.hex()}")
        bl.ser.close()
        return
    if not args.image:
        ap.error("image required (or --selftest)")
    flash_unit(args.image, uid=args.uid, bl_mode=args.bl_mode)


if __name__ == "__main__":
    main()
