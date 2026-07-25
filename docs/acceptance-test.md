# Build, flash, and acceptance record

## Exact commands

Run from the repository root in PowerShell:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device list

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test `
  -e esp32-s3-tests -f test_core --upload-port COM9 --test-port COM9

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run `
  -e esp32-s3-devkitc-1
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run `
  -e esp32-s3-devkitc-1 --target upload --upload-port COM9

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor `
  --port COM9 --baud 115200
```

The local `include/secrets.h` is ignored. Do not paste its passwords into
captured logs or commits.

## Real-device record

Results from 2026-07-26:

- [x] Production-linked target tests passed: 30/30
- [x] Normal application rebuilt and flashed automatically through COM9
- [x] Flash images were hash verified and the board hard-reset through RTS
- [ ] Live electrical-low samples produce logical `true` after active-low normalization
- [x] Live unloaded/electrical-high samples produced logical GPIO/group values `false`
- [x] Wi-Fi connected to `stillwork`
- [ ] DHCP address in `192.168.51.10`–`192.168.51.100`
- [x] Authenticated MQTT connection to `192.168.51.1:1884`; state publishes were queued
- [ ] Retained availability and state observed by the Orange Pi
- [ ] AP/broker interruption and recovery verified end to end

The application build used 46,588 bytes RAM (14.2%) and 848,405 bytes flash
(25.4% of the configured application partition).

The lifecycle target harness produced:

```text
[LifecycleTest] edge=connected availability_due=1 state_due=1
[LifecycleTest] edge=disconnected then=connected availability_due=1 state_due=1
[LifecycleTest] edge=reconnected availability_due=1 state_due=1
30 Tests 0 Failures 0 Ignored
```

This verifies ordered transition reduction and immediate availability/current
state scheduling. It does not replace the pending real AP, DHCP, broker, LWT,
and reconnect checks.

The local ignored credentials were checked without printing them. After the
active-low fix, live serial output showed unloaded logical values
`g4=0 g5=0 g6=0 g7=0 cushion=0 footrest=0 presence=0`; the corresponding
retained QoS 1 state publications were accepted by the MQTT client for queuing.

The ESP32 keeps the required simple boundary: each physical GPIO is active-low
and is normalized at sampling so every outward logical value remains
`1`/`true` = triggered. GPIO4/GPIO5 form cushion by OR, GPIO6/GPIO7 form
footrest by OR, and the Orange Pi—not this firmware—owns staleness and the
3-second `NIGHT_EXEC` dwell.
