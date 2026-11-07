# Bench area

Records for the V2 protocol migration. `firmware/V2_MIGRATION_PLAN.md` is the
master plan; this directory holds transcripts and rollback binaries.

## Phase 0.2 — v1 baseline capture (run before any firmware changes)

With **Unit A = ground station** plugged into the PC (`/dev/ttyACM0` or similar)
and **Unit B = tracker** flashed with `-DAPP_BOOT_MODE=TRACKER`, GPS + battery:

```sh
# A alone (B absent):
python3 tools/v2bench.py --port /dev/ttyACM0 --v1smoke --no-scan \
        --transcript tools/bench/v1-baseline.txt

# Both units powered (B on the bench, ~2 m from A):
python3 tools/v2bench.py --port /dev/ttyACM0 --v1smoke \
        --transcript tools/bench/v1-baseline.txt

# Optional manual discovery transcript (`cmd`-free, by hand):
python3 tools/v2bench.py --port /dev/ttyACM0 --v1smoke --no-scan
# then in the console use `hex` to send raw lines like "SCAN\n" if needed
```

Phases 1.x re-run these exact commands; the transcript diff (field counts,
`b` default) is the regression gate. Then commit the baseline transcripts
together with the phase work as part of the audit trail.
