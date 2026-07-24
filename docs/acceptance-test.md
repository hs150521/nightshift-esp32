# Build, flash, and acceptance record

## Exact commands

All commands run from the repository root in PowerShell:

```powershell
# Tool and device discovery
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" --version
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device list

# Read-only board probe
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
  "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
  --chip esp32s3 --port COM9 flash_id

# Production-linked target tests
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test `
  -e esp32-s3-tests -f test_core --upload-port COM9 --test-port COM9

# Build and restore application firmware
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run `
  -e esp32-s3-devkitc-1
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run `
  -e esp32-s3-devkitc-1 --target upload

# Serial monitor
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor `
  --port COM9 --baud 115200
```

The local `include/secrets.h` is ignored. Do not paste passwords into this
record or captured logs.

## Real-device checklist

Results from 2026-07-25:

- [x] Application flashed automatically through COM9
- [x] Serial boot line includes firmware version and a fresh boot ID
- [ ] Wi-Fi connected to `stillwork` — AP returned repeated `AUTH_EXPIRE`
- [ ] DHCP address is in `192.168.51.10`–`192.168.51.100`
- [ ] Authenticated MQTT connection to `192.168.51.1:1884`
- [ ] Retained online availability observed
- [ ] Retained state observed at approximately 3-second intervals
- [ ] GPIO4 high produces cushion true — covered by target test; not physically driven
- [ ] GPIO5 high with GPIO4 low keeps cushion true — target test only
- [x] Live GPIO6/GPIO7 levels produced `footrest=true`
- [x] Live all-low snapshot produced all groups false
- [ ] Broker/AP interruption recovery — reconnect scheduling observed, full link unavailable
- [x] Automatic reset produced a new boot ID (`687618c9` then `8fae3d33`)

PlatformIO target result: **20/20 passed**. Final application build used 46,580
bytes RAM (14.2%) and 847,861 bytes flash (25.4% of the configured application
partition). Automatic upload wrote and hash-verified all images, then hard-reset
through RTS.

The local credentials were checked against the supplied values without printing
them. Serial showed `Reason: 2 - AUTH_EXPIRE`, and the upper computer also had no
route to `192.168.51.1:1884`. Consequently DHCP, broker authentication, retained
messages, and Orange Pi state-machine integration could not be evidenced in this
run.

The physical GPIO stimuli and Orange Pi authoritative state-machine behavior
require access to the sensor wiring/Orange Pi observer. The ESP32 must not
implement or simulate the Orange Pi's 3-second `NIGHT_EXEC` release dwell.
