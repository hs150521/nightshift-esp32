"""Capture a bounded COM9 log and optionally reset through DTR/RTS."""

from __future__ import annotations

import argparse
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM9")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=20.0)
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as connection:
        if args.reset:
            # Keep GPIO0 released (DTR false) and pulse EN via RTS.
            connection.dtr = False
            connection.rts = True
            time.sleep(0.1)
            connection.rts = False
            time.sleep(0.4)

        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            raw = connection.readline()
            if not raw:
                continue
            print(raw.decode("utf-8", errors="replace").rstrip(), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
