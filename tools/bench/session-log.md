# Bench session log (standing practice)

Appended chronologically for every bench investigation going forward.
Format per entry: timestamp, unit port map (UID<->port), firmware image id/size,
commands sent + raw responses, interpretation notes. Complements
`v1-baseline.txt` (which holds the Phase-0.2 anchor captures).

---

/dev/ttyACM0 -> d0f85e68
/dev/ttyACM1 -> ecceb7e8
map: {'d0f85e68': '/dev/ttyACM0', 'ecceb7e8': '/dev/ttyACM1'}
A radio-probe dump:


## 2026-11-07 21:50 — post-review state check
- Ports: ttyACM0=d0f85e68 (A/GS), ttyACM1=ecceb7e8 (B/tracker).
- A flashed @21:46 with probe build (text 31,348, radio register probe + lean trace).
- A DBG response: EMPTY ('\\n' only) — no trace contents. Expected EA/EC register
  tokens if the 21:46 probe build is actually resident. Suspect: A's image is
  stale (pre-probe) OR probe path did not complete. Resolving via B comparison.
- B DBG responds with F7/F8-style trace -> B image = 21:41 build (IRQ traces +
  SpiCmd critical-section), WITHOUT register probe. A's empty DBG response ambiguous:
  either stale image on A or probe path not completing -> must flash BOTH units to
  the same 21:46 image (probe + lean trace) before any register comparison.
- Next: B flash round, then re-verify A's DBG, then register comparison + full battery.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31740
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31740 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31740 bytes
Download done.
File downloaded successfully
## 2026-11-07 22:02 — VARIANT MATRIX (one variable at a time)
- Bflash @21:44 confirmed: DBG yields EA/EC register tokens; radio status byte
  consistent with RX-mode chip. B is a working receiver.
- A's DBG returns EMPTY both calls -> A's earlier flash state untrusted
  (dfu-util does not verify content). A needs a clean flash of any test build.
- Register readback abandoned: probe register addresses were picked from memory
  (freq 0x08E7, CRC 0x0631) and semantics unverified -> do not rely on their values.
- Variant 1 built (/tmp/opencode/variant1.bin, 31,736 B): SpiCmd critical section
  REVERTED; STANDBY fix kept; radio probe + lean trace kept.
- Battery per variant (same script): B->A probe (T), A->B SCAN->D, PAIR->R, x5
  repetitions each. Pass/fail rates logged. Variant 2 = current build (critical
  section in) for comparison.
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31736
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31736 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31736 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31736
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31736 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31736 bytes
Download done.
File downloaded successfully
===== VARIANT 1 battery (critical-section OFF, standby fix ON) =====
[0] B->A: PASS 'ecceb7e8 variant1-0\x100 -26'
[1] B->A: PASS 'ecceb7e8 variant1-001 -25'
[2] B->A: PASS 'ecceb7e8 variant1-00\x12 -25'
[3] B->A: PASS 'ecceb7e8 variant1-003 -26'
[4] B->A: PASS 'ecceb7e8 variant1-0\x104 -25'
[0] SCAN->D: FAIL '0'
[1] SCAN->D: PASS '1 ecceb7e8,-26'
[2] SCAN->D: FAIL '0'
[3] SCAN->D: FAIL '0'
[4] SCAN->D: FAIL '0'
[0] PAIR->ACK: PASS 'ecceb7e8 &d0f85e6=UeCceb\x07e8'
[1] PAIR->ACK: PASS 'ecceb7e8 &d0f85e6=UeCceb\x07e8'
[2] PAIR->ACK: PASS 'ecceb7e8 &d0f85e6=UeCceb\x07e8'
[3] PAIR->ACK: PASS 'ecceb7e8 &d0f85e6=UeCceb\x07e8'
[4] PAIR->ACK: PASS 'ecceb7e8 &d0f85e6=UeCceb\x07e8'
VARIANT 1 SUMMARY: {'b2a': 5, 'disc': 1, 'ack': 5}
## 2026-11-07 22:10 — VARIANT 1 battery results (both units, identical image)
- B->A probes: 5/5 PASS (RSSI -25..-26). Byte corruption present in EVERY burst
  (1-2 bytes, e.g. 'variant1-0\x100') -> v1 air plane has NO working CRC
  integrity; corrupted bytes pass straight into app-layer parsing (mis-parse
  branches flip roles / Corrupt ACK reads). H1-class gap; v2 air plane fixes via
  LEN + CRC16.
- SCAN->D discovery: 1/5 (reply timing / slot delays flaky in v1 - known L-item
  family, v2 replaces plane entirely).
- PAIR->ACK: 5/5 (ACK captured each attempt; same garbled string stored last -
  criterion was non-empty R; STRICT ACK-content matching still distorted by the
  1-2 byte corruption).
- CONCLUSION: Variant 1 (critical section OFF, STANDBY fix ON) is the better
  bench build and is now the reference image for continued phases.
- NEXT: units separated >=2 m again; one confirmation battery; optional bench
  hardening = enable SX1262 HW CRC (crcEnabled=1 TX+RX SetPacketParams) as a
  v1 stopgap; then strip all TEMP-DEBUG, clean build, capture final Phase-0.2
  baseline, commit in logical groups.
## 2026-11-07 22:14 — Design intent clarification (authoritative, from author)
- crcEnabled=0 on air is DELIBERATE: goal = weak-packet data recovery (don't
  discard a packet for a single bad bit). The recovery layer itself was never
  implemented, so corrupted bytes currently reach app parsers unchecked.
- Consequence for V2 air plane (Phase 3): keep this goal explicitly - options:
  (a) SX1262 HW CRC off + app-level CRC16 in every frame, corrupt frames
      delivered with a no的数据 flag rather than silently garbled; or
  (b) HW CRC on + last-try raw pass. Decision goes in docs/COMMUNICATION_SPEC_V2.md
      as a Phase-3 input, NOT a silent change.
- Units were re-joined (<1 m) for the DFU flashing dance (author's own note);
  battery RSSI -25/-26 = saturation range -> frame corruption expected there.
  Author re-separating >=2 m; confirmation battery follows.
===== VARIANT 1 confirmation battery @ separated units =====
[0] B->A: PASS 'ecceb7e8 confirm-00\x10 -52'
[1] B->A: PASS 'ecceb7e8 confirm-00\x11 -51'
[2] B->A: PASS 'ecceb7e8 confirm-002 -47'
[3] B->A: PASS 'ecceb7e8 confirm-003 -48'
[4] B->A: PASS 'ecceb7e8 confirm-004 -51'
[0] SCAN->D: FAIL '0'
[1] SCAN->D: FAIL '0'
[2] SCAN->D: FAIL '0'
[3] SCAN->D: FAIL '0'
[4] SCAN->D: FAIL '0'
[0] PAIR->ACK: FAIL 'None'
[1] PAIR->ACK: FAIL 'None'
[2] PAIR->ACK: FAIL 'None'
[3] PAIR->ACK: FAIL 'None'
[4] PAIR->ACK: FAIL 'None'
CONFIRMATION SUMMARY: {'b2a': 5, 'disc': 0, 'ack': 0}
## 2026-11-07 22:18 — ROOT CAUSE #2 identified: TX power never configured
- Separated battery: B->A 5/5 (RSSI -47..-52, still 1 byte/bit corruption per
  burst - 0x30->0x10-class single-bit flips in the same payload position);
  A->B died completely at range (D 0/5, ACK 0/5). At <1 m A->B worked (ACK 5/5).
- Saturation theory REJECTED (would not explain range-dependent RX).
- Smoking gun: CMD_SET_TX_PARAMS (0x8E) defined in lora.c but NEVER ISSUED.
  SX1262 TX power = power-on default (unit-dependent). A defaults very low;
  B workable -> full asymmetric link behavior explained.
- FIX: SetTxParams(+10 dBm, 200us ramp) added into LoRa_ApplyConfig.
- Payload corruption (bit-flips) is a separate, still-open RF integrity issue;
  crcEnabled=0 is a deliberate design input for V2 (app CRC + corruption flag).
- Flash budget dance: text 31,336->31,348 by trimming F1/F2/E1 debug entries.
  Variant 1b built (31,748 B bin).
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31736
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31736 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31736 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31736
Erase   	[                         ]   0%            0 bytesErase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31736 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31736 bytes
Download done.
File downloaded successfully
===== VARIANT 1b battery (SetTxParams fix, both units) =====
[0] B->A: FAIL ''
[1] B->A: FAIL ''
[2] B->A: FAIL ''
[3] B->A: FAIL ''
[4] B->A: FAIL ''
[0] SCAN->D: PASS '1 ecceb7e8,-52'
[1] SCAN->D: PASS '1 ecceb7e8,-51'
[2] SCAN->D: FAIL '0'
[3] SCAN->D: PASS '1 ecceb7e8,-52'
[4] SCAN->D: PASS '1 ecceb7e8,-51'
[0] PAIR->ACK: FAIL 'None'
[1] PAIR->ACK: FAIL 'None'
[2] PAIR->ACK: FAIL 'None'
[3] PAIR->ACK: FAIL 'None'
[4] PAIR->ACK: FAIL 'None'
VARIANT 1b SUMMARY: {'b2a': 0, 'disc': 4, 'ack': 0}
## 2026-11-07 22:25 — VARIANT 1b battery (partial autonomy + harness bugs)
- SCAN->D at range: 4/5 PASS (RSSI -51/-52) at SetTxParams(+10dBm) both units
  -> A->B link WORKS at range now. Root cause #2 confirmed fixed.
- B->A leg 0/5 = TEST BUG: probe payloads 'v1b-000' < 10 chars are silently
  ERR'd by firmware 'T' handler. Not a link fault. Future probes: >=10 chars.
- PAIR->ACK 0/5 explained: corrupted-ACK payload (single-bit flips) fails
  strncmp("ACK") on A's T handler -> A silently flips to TRACKER runtime mode ->
  all subsequent ACK handling disabled until reboot. Mode-state cascade in the
  same battery. (Full R-string readbacks below will document actual content.)
- CLEAN-SLATE retest queued with: >=10-char probes; 1 PAIR per fresh state;
  raw R strings captured; mode-state tracked.
===== CLEAN-SLATE calibrated battery Sat Nov  7 22:23:19 2026 =====
[B->A 0] txReply='OK' R='ecceb7e8 remote-probe-000 -53'
[B->A 1] txReply='OK' R='ecceb7e8 remote-probe-001 -53'
[B->A 2] txReply='OK' R='ecceb7e8 remote-probe-002 -53'
PAIR resp: 'OK'
  R poll 0: 'ecceb7e8 &d0f85e68T!CK'
  R poll 1: 'ecceb7e8 &d0f85e68T!CK'
  R poll 2: 'ecceb7e8 &d0f85e68T!CK'
  R poll 3: 'ecceb7e8 &d0f85e68T!CK'
## 2026-11-07 22:26 — Root cause #3 (the real TX power fix)
- lora_init copies setting 'd' (txPower, default LORA_TX_POWER_22_DBM) into
  hlora->txPower, but SetTxParams was never issued anywhere in the driver ->
  the setting never reached the radio. Radios TX at per-unit power-on defaults
  (A unit: only <1 m reach; B unit: workable). Explains ALL asymmetric link
  behavior observed tonight (A->B range failure while B->A fine).
- FIX: LoRa_ApplyConfig now issues SetTxParams((uint8_t)l->txPower, ramp 0x04)
  - uses the authored setting, not a hardcoded value. Variant 1c built
  (text 31,340, bin 31,748).
- ALSO documented: single-bit-5 flips ('0'->0x10, 'A'->'!') one byte per burst,
  stable in memory, RF-independent-looking; coding rate is CR_4_5 so HW FEC
  should correct single-bit errors -> occurrence is NOT explained by air noise.
  Still-open, tracked. 10x same-payload hex-capture test queued to localize.
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31732
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31732 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31732 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31732
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  12%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  25%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  38%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[============             ]  51%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  64%        20480 bytesErase   	[=================        ]  70%        22528 bytesErase   	[===================      ]  77%        24576 bytesErase   	[====================     ]  83%        26624 bytesErase   	[======================   ]  90%        28672 bytesErase   	[======================== ]  96%        30720 bytesErase   	[=========================] 100%        31732 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  12%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  25%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  38%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[============             ]  51%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  64%        20480 bytesDownload	[=================        ]  70%        22528 bytesDownload	[===================      ]  77%        24576 bytesDownload	[====================     ]  83%        26624 bytesDownload	[======================   ]  90%        28672 bytesDownload	[======================== ]  96%        30720 bytesDownload	[=========================] 100%        31732 bytes
Download done.
File downloaded successfully
===== VARIANT 1c battery (txPower=+22dBm authored, both units) Sat Nov  7 22:30:49 2026
-- Leg 1: 10x identical B->A payload, hex-captured --
[0] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 33 34 20 2D 34 38
[1] PASS 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 32 33 34 20 2D 35 34
[2] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 37 34 20 2D 34 38
[3] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 30 32 33 34 20 2D 34 38
[4] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 33 34 20 2D 34 38
[5] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 37 34 20 2D 34 38
[6] PASS 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 32 33 34 20 2D 35 30
[7] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 33 34 20 2D 34 38
[8] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 37 34 20 2D 34 39
[9] CORRUPT 65 63 63 65 62 37 65 38 20 54 45 53 54 53 54 52 49 4E 47 31 92 33 34 20 2D 35 31
-- Leg 2: SCAN->D x5 --
[0] FAIL '0'
[1] FAIL '0'
[2] FAIL '0'
[3] FAIL '0'
[4] PASS '1 ecceb7e8,-52'
-- Leg 3: PAIR->ACK x5, raw ACK hex --
[0] CORRUPT/FAIL 65 63 63 65 62 37 65 38 20 26 64 30 66 38 35 65 36 38 55 65 63 63 67 62 05 65 38
[1] CORRUPT/FAIL 65 63 63 65 62 37 65 38 20 26 64 30 66 38 35 65 36 38 55 65 63 63 67 62 05 65 38
[2] CORRUPT/FAIL 65 63 63 65 62 37 65 38 20 26 64 30 66 38 35 65 36 38 55 65 63 63 67 62 05 65 38
[3] CORRUPT/FAIL 65 63 63 65 62 37 65 38 20 26 64 30 66 38 35 65 36 38 55 65 63 63 67 62 05 65 38
[4] CORRUPT/FAIL 65 63 63 65 62 37 65 38 20 26 64 30 66 38 35 65 36 38 55 65 63 63 67 62 05 65 38
VARIANT 1c SUMMARY: {'b2a': 2, 'disc': 1, 'ack': 0}
## 2026-11-07 22:45 — Wrap-up
- All TEMP-DEBUG stripped (lora.c, lora_arbitration.{c,h}, event.c, commands.{c,h},
  main.c, task_lora_rx.c). One regex mishap (deleted SpiCmd/LoRa_ClearIrq and
  more) recovered by restoring lora.c from git HEAD and re-applying the two
  real fixes cleanly.
- Clean build: text 30,952 / bin 31,344; static RAM 4,328 (map _ebss
  0x200010E8) -> baseline margin restored (792 B unreserved after 1,024 stack).
- Units remain flashed with the diagnostic image; clean build flashes next
  session (2 DFU rounds). Definitive Phase 0.2 capture happens on the clean build.
- Next-session brief written: tools/bench/corruption-prep.md
dfu-util -a 0 -D build_linktest/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Could not open file build_linktest/firmware.bin for reading: No such file or directory
make: *** [Makefile:220: program_lt] Error 66
make: *** No rule to make target 'build/.bin', needed by 'program_lt'.  Stop.
dfu-util -a 0 -D build_linktest/firmware_lt.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 21748
Erase   	[                         ]   0%            0 bytesErase   	[==                       ]   9%         2048 bytesErase   	[====                     ]  18%         4096 bytesErase   	[=======                  ]  28%         6144 bytesErase   	[=========                ]  37%         8192 bytesErase   	[===========              ]  47%        10240 bytesErase   	[==============           ]  56%        12288 bytesErase   	[================         ]  65%        14336 bytesErase   	[==================       ]  75%        16384 bytesErase   	[=====================    ]  84%        18432 bytesErase   	[=======================  ]  94%        20480 bytesErase   	[=========================] 100%        21748 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[==                       ]   9%         2048 bytesDownload	[====                     ]  18%         4096 bytesDownload	[=======                  ]  28%         6144 bytesDownload	[=========                ]  37%         8192 bytesDownload	[===========              ]  47%        10240 bytesDownload	[==============           ]  56%        12288 bytesDownload	[================         ]  65%        14336 bytesDownload	[==================       ]  75%        16384 bytesDownload	[=====================    ]  84%        18432 bytesDownload	[=======================  ]  94%        20480 bytesDownload	[=========================] 100%        21748 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build_linktest/firmware_lt.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 21748
Erase   	[                         ]   0%            0 bytesErase   	[                         ]   0%            0 bytesErase   	[==                       ]   9%         2048 bytesErase   	[====                     ]  18%         4096 bytesErase   	[=======                  ]  28%         6144 bytesErase   	[=========                ]  37%         8192 bytesErase   	[===========              ]  47%        10240 bytesErase   	[==============           ]  56%        12288 bytesErase   	[================         ]  65%        14336 bytesErase   	[==================       ]  75%        16384 bytesErase   	[=====================    ]  84%        18432 bytesErase   	[=======================  ]  94%        20480 bytesErase   	[=========================] 100%        21748 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[==                       ]   9%         2048 bytesDownload	[====                     ]  18%         4096 bytesDownload	[=======                  ]  28%         6144 bytesDownload	[=========                ]  37%         8192 bytesDownload	[===========              ]  47%        10240 bytesDownload	[==============           ]  56%        12288 bytesDownload	[================         ]  65%        14336 bytesDownload	[==================       ]  75%        16384 bytesDownload	[=====================    ]  84%        18432 bytesDownload	[=======================  ]  94%        20480 bytesDownload	[=========================] 100%        21748 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build_linktest/firmware_lt.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 21748
Erase   	[                         ]   0%            0 bytesErase   	[==                       ]   9%         2048 bytesErase   	[====                     ]  18%         4096 bytesErase   	[=======                  ]  28%         6144 bytesErase   	[=========                ]  37%         8192 bytesErase   	[===========              ]  47%        10240 bytesErase   	[==============           ]  56%        12288 bytesErase   	[================         ]  65%        14336 bytesErase   	[==================       ]  75%        16384 bytesErase   	[=====================    ]  84%        18432 bytesErase   	[=======================  ]  94%        20480 bytesErase   	[=========================] 100%        21748 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[==                       ]   9%         2048 bytesDownload	[====                     ]  18%         4096 bytesDownload	[=======                  ]  28%         6144 bytesDownload	[=========                ]  37%         8192 bytesDownload	[===========              ]  47%        10240 bytesDownload	[==============           ]  56%        12288 bytesDownload	[================         ]  65%        14336 bytesDownload	[==================       ]  75%        16384 bytesDownload	[=====================    ]  84%        18432 bytesDownload	[=======================  ]  94%        20480 bytesDownload	[=========================] 100%        21748 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build_linktest/firmware_lt.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 21748
Erase   	[                         ]   0%            0 bytesErase   	[==                       ]   9%         2048 bytesErase   	[====                     ]  18%         4096 bytesErase   	[=======                  ]  28%         6144 bytesErase   	[=========                ]  37%         8192 bytesErase   	[===========              ]  47%        10240 bytesErase   	[==============           ]  56%        12288 bytesErase   	[================         ]  65%        14336 bytesErase   	[==================       ]  75%        16384 bytesErase   	[=====================    ]  84%        18432 bytesErase   	[=======================  ]  94%        20480 bytesErase   	[=========================] 100%        21748 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[==                       ]   9%         2048 bytesDownload	[====                     ]  18%         4096 bytesDownload	[=======                  ]  28%         6144 bytesDownload	[=========                ]  37%         8192 bytesDownload	[===========              ]  47%        10240 bytesDownload	[==============           ]  56%        12288 bytesDownload	[================         ]  65%        14336 bytesDownload	[==================       ]  75%        16384 bytesDownload	[=====================    ]  84%        18432 bytesDownload	[=======================  ]  94%        20480 bytesDownload	[=========================] 100%        21748 bytes
Download done.
File downloaded successfully
make: *** No rule to make target 'build/firmware_lt.bin', needed by 'program_lt'.  Stop.
dfu-util -a 0 -D build_linktest/firmware_lt.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 22072
Erase   	[                         ]   0%            0 bytesErase   	[==                       ]   9%         2048 bytesErase   	[====                     ]  18%         4096 bytesErase   	[======                   ]  27%         6144 bytesErase   	[=========                ]  37%         8192 bytesErase   	[===========              ]  46%        10240 bytesErase   	[=============            ]  55%        12288 bytesErase   	[================         ]  64%        14336 bytesErase   	[==================       ]  74%        16384 bytesErase   	[====================     ]  83%        18432 bytesErase   	[=======================  ]  92%        20480 bytesErase   	[=========================] 100%        22072 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[==                       ]   9%         2048 bytesDownload	[====                     ]  18%         4096 bytesDownload	[======                   ]  27%         6144 bytesDownload	[=========                ]  37%         8192 bytesDownload	[===========              ]  46%        10240 bytesDownload	[=============            ]  55%        12288 bytesDownload	[================         ]  64%        14336 bytesDownload	[==================       ]  74%        16384 bytesDownload	[====================     ]  83%        18432 bytesDownload	[=======================  ]  92%        20480 bytesDownload	[=========================] 100%        22072 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build_linktest/firmware_lt.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 22072
Erase   	[                         ]   0%            0 bytesErase   	[==                       ]   9%         2048 bytesErase   	[====                     ]  18%         4096 bytesErase   	[======                   ]  27%         6144 bytesErase   	[=========                ]  37%         8192 bytesErase   	[===========              ]  46%        10240 bytesErase   	[=============            ]  55%        12288 bytesErase   	[================         ]  64%        14336 bytesErase   	[==================       ]  74%        16384 bytesErase   	[====================     ]  83%        18432 bytesErase   	[=======================  ]  92%        20480 bytesErase   	[=========================] 100%        22072 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[==                       ]   9%         2048 bytesDownload	[====                     ]  18%         4096 bytesDownload	[======                   ]  27%         6144 bytesDownload	[=========                ]  37%         8192 bytesDownload	[===========              ]  46%        10240 bytesDownload	[=============            ]  55%        12288 bytesDownload	[================         ]  64%        14336 bytesDownload	[==================       ]  74%        16384 bytesDownload	[====================     ]  83%        18432 bytesDownload	[=======================  ]  92%        20480 bytesDownload	[=========================] 100%        22072 bytes
Download done.
File downloaded successfully
===== LINKTEST corruption experiment #1 (dual roles) =====
-- Phase A: U0 TXTEST -> U1 CAP/MAP --
TXTEST -> OK
MAP n=10 clean=0 corrupt=8 mismatch=0 rssi=-36..-36 snr=4..7
pos 38 xor=0xC0 hits=2
pos 39 xor=0x04 hits=1
pos 43 xor=0x20 hits=1
pos
-- Phase B: U1 TXTEST -> U0 CAP/MAP --
TXTEST -> OK
MAP n=8 clean=0 corrupt=8 mismatch=0 rssi=-37..-37 snr=4..10
pos 21 xor=0x04 hits=1
pos 24 xor=0x53 hits=1
pos 25 xor=0x80 hits=1
pos
== repeat MAPs verbosely (raw, full) ==
U1 MAP (A->B direction):
MAP n=9 clean=0 corrupt=8 mismatch=0 rssi=-37..-36 snr=3..10
pos 29 xor=0xC0 hits=1
pos 33 xor=0x02 hits=1
pos 34 xor=0x49 hits=1
pos
U0 MAP (B->A direction):
MAP n=6 clean=0 corrupt=6 mismatch=0 rssi=-37..-37 snr=4..10
pos 15 xor=0x60 hits=1
pos 17 xor=0x08 hits=1
pos 21 xor=0x04 hits=1
pos
== raw per-burst capture (U1 RXS on, U0 TXTEST 10) ==
captured 0 frames; showing consecutive frames' bytes 12-45:
== raw per-burst capture attempt #2 (RXS on both tools' path) ==
RXS: ON
RAW U1 stream:
[RX 229941 rssi=-36 snr=10 len=64 hex=32 00 A5 5A 3C C3 00 FF 0F F0 18 E7 80 7F 01 FE 40 B7 20 DF 10 E3 08 F7 07 DB 02 FD 31 F6 C9 F[RX 231091 rssi=-36 snr=10 len=64 hex=33 00 A5 5A 3C C3 00 FF 0F F0 18 E7 80 7F 01 FE 40 BF 20 DF 10 E7 08 F7 04 DB 02 FD 01 36 C9 F[RX 232240 rssi=-37 snr=6 len=64 hex=34 00 A5 5A 3C C3 00 FF 0F F0 18 E7 80 7F 01 FE 40 BF 20 DF 10 E7 08 F7 04 FB 02 FD 01 36 C9 FF[RX 233390 rssi=-37 snr=8 len=64 hex=35 00 A5 5A 3C C3 00 FF 0F F0 18 E7 80 7F 01 FE 40 BF 20 DF 10 E7 08 F7 04 FB 02 FD 01 36 C9 FF[RX 234540 rssi=-37 snr=5 len=64 hex=36 00 A5 5A 3C C3 00 FF 0F F0 18 E7 80 7F 01 FE 40 BF 20 DF 10 E7 08 F7 04 FB 02 FD 01 36 C9 FF
===== SF sweep corruption test =====
-- A->B SF9 (baseline) --
MAP n=8 clean=0 corrupt=8 mismatch=0 rssi=-36..-36 snr=4..10
pos 21 xor=0x04 hits=1
pos 24 xor=0x53 hits=1
pos 25 xor=0x20 hits=1
pos
-- A->B SF12 --
MAP n=0
-- B->A SF12 --
MAP n=0
-- A->B SF7 (fastest) --
MAP n=15 clean=8 corrupt=0 mismatch=0 rssi=-35..-34 snr=11..12
-- B->A SF7 --
MAP n=7 clean=7 corrupt=0 mismatch=0 rssi=-35..-34 snr=11..12
restored SF9 both
===== BW sweep at SF9 =====
A->B SF9 BW62.5 -> MAP n=9 clean=0 corrupt=8 mismatch=0 rssi=-34..-33 snr=4..9
pos 29 xor=0xC0 hits=1
pos 34 xor=0x49 hits=1
pos 37 xor=0x20 hits=1
pos 3
A->B SF9 BW125 -> MAP n=14 clean=8 corrupt=0 mismatch=0 rssi=-35..-34 snr=10..10
B->A SF9 BW125 -> MAP n=15 clean=8 corrupt=0 mismatch=0 rssi=-34..-34 snr=9..10
A->B SF8 BW62.5 -> MAP n=13 clean=8 corrupt=0 mismatch=0 rssi=-34..-34 snr=10..11
B->A SF8 BW62.5 -> MAP n=14 clean=8 corrupt=0 mismatch=0 rssi=-35..-34 snr=8..10
restored SF9 BW62.5 both
## 2026-11-08 09:24 — Linktest SF/BW sweep: corruption isolated to SF9/BW62.5
Both units on identical linktest image (LT v1 2026-11-08 09:17). ERC pattern TX.
- SF9/BW62.5:  17 receipts, 0 clean, 100% corrupt frames. Multi-bit XoRs
  (0x49/0x53/0xC0-class) at drifting center positions (15..43), RSSI -34..-37,
  SNR +4..+10 (radio says demod quality GOOD while bits flip).
- SF7/BW62.5: 22 receipts, all clean (0 corrupt).
- SF8/BW62.5: 27 receipts, all clean.
- SF9/BW125:  29 receipts, all clean (same SF9, doubled bandwidth).
- SF12/BW62.5 (LDRO off both): 0 received (mutual mod-mismatch - consistent).
CONCLUSION: corruption is a deterministic function of SYMBOL TIME ONLY
(symbol ~8.2 ms bad, <=4.1 ms perfect) - NOT distance, noise, SPI reads,
host-side, TX-path framing, or per-unit hardware. Rules OUT the earlier
hypotheses (SPI readbacks, tx framing, CDC). Crystal drift argument fails:
SF change alone (same boards, same BW) flips the behavior within seconds.
NEXT: check SX1262 datasheet known-limitations/errata symbol-time class;
test SetRxBoosted vs SetRx; test LDRO=1 at SF9/62.5; note SF9/BW62.5 air
time per 64-B burst ~278 ms (TXTEST gap 350 ms leaves ~70 ms inter-burst).
===== hypothesis 1: LDRO at SF9/BW62.5 =====
U0: f=434000000 s=9 b=0x03 c=1 d=22 p=8 w=1424 i=0 l=1 t=0
A->B SF9 LDRO1 -> MAP n=5 clean=5 corrupt=0 mismatch=0 rssi=-34..-34 snr=10..13
B->A SF9 LDRO1 -> MAP n=8 clean=8 corrupt=0 mismatch=0 rssi=-34..-32 snr=8..12
===== control: LDRO back OFF at SF9/BW62.5 =====
A->B SF9 LDRO0 -> MAP n=9 clean=0 corrupt=8 mismatch=0 rssi=-35..-34 snr=4..8
pos 25 xor=0x20 hits=1
pos 28 xor=0x30 hits=1
pos 29 xor=0xC0 hits=1
pos 3
B->A SF9 LDRO0 -> MAP n=10 clean=0 corrupt=8 mismatch=0 rssi=-34..-32 snr=4..8
pos 29 xor=0xC0 hits=2
pos 33 xor=0x02 hits=1
pos 34 xor=0x09 hits=3
pos
===== restore LDRO=1 + sanity long-run =====
A->B SF9 LDRO1 n=30 -> MAP n=13 clean=8 corrupt=0 mismatch=0 rssi=-34..-33 snr=7..8
B->A SF9 LDRO1 n=30 -> MAP n=6 clean=6 corrupt=0 mismatch=0 rssi=-33..-33 snr=8..13
## 2026-11-08 10:05 — ROOT CAUSE #3 CONFIRMED: LDRO required at SF9/BW62.5
- Causal chain sealed with linktest battery: CFG l 1 => 13 receipts 100% clean
  (A->B and B->A); control CFG l 0 => corruption returns 8/8 frames both ways;
  long-run n=30 sanity clean (ring clamps MAP n to 8 - display quirk only).
- Rule (rule-of-thumb + bench): LDRO=1 when symbol time >= ~8.1 ms.
  Datasheet mandates LDRO only >16 ms; this silicon degrades earlier.
- Fix implemented in lora.c LoRa_ApplyConfig: auto-computes ldroEnabled from
  SF/BW symbol time (>=8.1 ms -> 1). Note: enum has BW_10/20 alias duplicates
  (0x08) and BW_20 mis-keyed (SX1262 20.8k = 0x09); if/else chain handles it.
- Corruption hunt CLOSED. Three bench-verified root causes total:
  (1) STANDBY-before-TX missing; (2) SetTxParams never issued;
  (3) LDRO disabled at SF9/BW62.5. All fixed in firmware/core drivers.
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31480
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  13%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  26%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  39%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[=============            ]  52%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  65%        20480 bytesErase   	[=================        ]  71%        22528 bytesErase   	[===================      ]  78%        24576 bytesErase   	[=====================    ]  84%        26624 bytesErase   	[======================   ]  91%        28672 bytesErase   	[======================== ]  97%        30720 bytesErase   	[=========================] 100%        31480 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  13%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  26%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  39%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[=============            ]  52%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  65%        20480 bytesDownload	[=================        ]  71%        22528 bytesDownload	[===================      ]  78%        24576 bytesDownload	[=====================    ]  84%        26624 bytesDownload	[======================   ]  91%        28672 bytesDownload	[======================== ]  97%        30720 bytesDownload	[=========================] 100%        31480 bytes
Download done.
File downloaded successfully
dfu-util -a 0 -D build/firmware.bin -s 0x08000000
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

dfu-util: Warning: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release
Opening DFU capable USB device...
Device ID 0483:df11
Device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Interface #0 ...
Determining device status...
DFU state(10) = dfuERROR, status(10) = Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations
Clearing status
Determining device status...
DFU state(2) = dfuIDLE, status(0) = No error condition is present
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
Downloading element to address = 0x08000000, size = 31480
Erase   	[                         ]   0%            0 bytesErase   	[=                        ]   6%         2048 bytesErase   	[===                      ]  13%         4096 bytesErase   	[====                     ]  19%         6144 bytesErase   	[======                   ]  26%         8192 bytesErase   	[========                 ]  32%        10240 bytesErase   	[=========                ]  39%        12288 bytesErase   	[===========              ]  45%        14336 bytesErase   	[=============            ]  52%        16384 bytesErase   	[==============           ]  58%        18432 bytesErase   	[================         ]  65%        20480 bytesErase   	[=================        ]  71%        22528 bytesErase   	[===================      ]  78%        24576 bytesErase   	[=====================    ]  84%        26624 bytesErase   	[======================   ]  91%        28672 bytesErase   	[======================== ]  97%        30720 bytesErase   	[=========================] 100%        31480 bytes
Erase    done.
Download	[                         ]   0%            0 bytesDownload	[=                        ]   6%         2048 bytesDownload	[===                      ]  13%         4096 bytesDownload	[====                     ]  19%         6144 bytesDownload	[======                   ]  26%         8192 bytesDownload	[========                 ]  32%        10240 bytesDownload	[=========                ]  39%        12288 bytesDownload	[===========              ]  45%        14336 bytesDownload	[=============            ]  52%        16384 bytesDownload	[==============           ]  58%        18432 bytesDownload	[================         ]  65%        20480 bytesDownload	[=================        ]  71%        22528 bytesDownload	[===================      ]  78%        24576 bytesDownload	[=====================    ]  84%        26624 bytesDownload	[======================   ]  91%        28672 bytesDownload	[======================== ]  97%        30720 bytesDownload	[=========================] 100%        31480 bytes
Download done.
File downloaded successfully
===== PHASE 0.2 definitive capture (v1 with 3 driver fixes) Sun Nov  8 09:49:54 2026
PAIR -> OK
ACK: MISSING
R poll 0: ''
R poll 1: ''
R poll 2: ''
NOTE: GPS telemetry requires GPS fix on B (bench) - expected absent tonight
== sanity: B settings + radio state ==
== A: SCAN -> D (discovery leg) ==
D final: NONE
== A: re-PAIR immediately (within/after window) ==
PAIR: OK
R poll 0: ''
R poll 1: ''
R poll 2: ''
R poll 3: ''
map: {'ecceb7e8': '/dev/ttyACM0', 'd0f85e68': '/dev/ttyACM1'}
===== fresh-boot battery Sun Nov  8 09:54:12 2026
SCAN->D final: 0
PAIR: OK
R poll 0: ''
R poll 1: ''
R poll 2: ''
R poll 3: ''
(B settings above: f/s/b)
## 2026-11-08 10:12 — Session wrap (corruption hunt: root cause found & fixed on linktest; v1-path discord open)
- Swapped both units to the v1 app build (30,988 B, 3 driver fixes + auto-LDRO).
- v1smoke A-alone smoke v2bench: 10/10 no FAILs (UID/GET/SET/invalid-label checks fine).
- OPEN: v1 image at SF9/BW62.5 still corrupts ~1 byte per received frame
  ('CHECKME123' -> 'CHECK'); auto-LDRO rule presence/working on the v1 path
  unverified. Linktest image's LDRO=1 was bit-perfect, so next session must:
    1) reflash linktest (2 rounds), STAT-counter battery to split frame-loss
       vs frame-corruption (frame loss up to 70% of TXTEST bursts observed),
    2) verify v1-path LDRO computation (possible: rule evaluates differently
       on the v1 code path),
    3) v1-PCB gotcha list updated: GET m shows SETTING default (never runtime
       mode); GET without arg NULL-derefs (M3-class, hit in practice twice).
- The definitive v1 Phase-0.2 baseline capture still pending; the earlier
  10/10 v1smoke panel (A-alone) anchored today on tools/bench/v1-baseline.txt.
