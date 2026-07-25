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

Results from 2026-07-25:

- [x] Production-linked target tests passed: 29/29
- [x] Normal application rebuilt and flashed automatically through COM9
- [x] Flash images were hash verified and the board hard-reset through RTS
- [x] Serial boot reported firmware 0.1.0 and fresh boot ID `b31e4126`
- [x] Latest live GPIO4–GPIO7 sample was high, producing both grouped inputs true
- [x] Earlier live GPIO5-high/GPIO4-low sample produced `cushion=true`
- [x] Earlier live GPIO6/GPIO7 levels produced `footrest=true`
- [x] Earlier live all-low sample produced both groups false
- [ ] Wi-Fi connected to `stillwork`
- [ ] DHCP address in `192.168.51.10`–`192.168.51.100`
- [ ] Authenticated MQTT connection to `192.168.51.1:1884`
- [ ] Retained availability and state observed by the Orange Pi
- [ ] AP/broker interruption and recovery verified end to end

The application build used 46,588 bytes RAM (14.2%) and 848,305 bytes flash
(25.4% of the configured application partition).

The lifecycle target harness produced:

```text
[LifecycleTest] edge=connected availability_due=1 state_due=1
[LifecycleTest] edge=disconnected then=connected availability_due=1 state_due=1
[LifecycleTest] edge=reconnected availability_due=1 state_due=1
29 Tests 0 Failures 0 Ignored
```

This verifies ordered transition reduction and immediate availability/current
state scheduling. It does not replace the pending real AP, DHCP, broker, LWT,
and reconnect checks.

The local ignored credentials were checked without printing them. In the latest
simultaneous run, the ESP32 repeatedly reported `NO_AP_FOUND` while the Orange
Pi showed `ap0` down and `wlan0` associated on 5260 MHz. This localizes the
current blocker to the OPI shared-radio/AP lifecycle, before DHCP or MQTT
authentication.

The ESP32 keeps the required simple boundary: each GPIO is active-high,
GPIO4/GPIO5 form cushion by OR, GPIO6/GPIO7 form footrest by OR, and the Orange
Pi—not this firmware—owns staleness and the 3-second `NIGHT_EXEC` dwell.
