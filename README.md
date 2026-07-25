# Nightshift ESP32 Pressure Node

Reliable four-input digital pressure node for the Nightshift system.

**Device ID:** `pressure-01` | **Board:** ESP32-S3-DevKitC-1 (detected unit:
32 MB octal flash, 16 MB octal PSRAM) | **Firmware:** `0.1.0`

## Hardware Mapping

| ESP32-S3 GPIO | Meaning | Electrical Trigger Level |
|---|---|---|
| GPIO4 | Cushion left | Low = triggered |
| GPIO5 | Cushion right | Low = triggered |
| GPIO6 | Footrest left | Low = triggered |
| GPIO7 | Footrest right | Low = triggered |

**Logical groups:**

- `cushion = GPIO4 OR GPIO5`
- `footrest = GPIO6 OR GPIO7`
- `presence = cushion OR footrest`

All four pins are sampled from one GPIO input register approximately every
10 ms and use internal pull-ups. The hardware inputs are active-low, and the
sampling layer normalizes them before debounce and publication. All serial and
MQTT logical values therefore retain the contract `1`/`true` = triggered and
`0`/`false` = not triggered.

## Wi-Fi Configuration

| Parameter | Value |
|---|---|
| SSID | `stillwork` |
| Security | WPA2-PSK |
| Band | 2.4 GHz |
| Addressing | DHCP (192.168.51.10–100) |
| Gateway | 192.168.51.1 |

## MQTT Configuration

| Parameter | Value |
|---|---|
| Broker | 192.168.51.1:1884 |
| Username | `pressure-01` |
| Client ID | `pressure-01-<6-hex eFuse-MAC suffix>` |
| Keepalive | 30 seconds |

### Topics

```
nightshift/v1/sensor/pressure/pressure-01/availability  (QoS 1, retained)
nightshift/v1/sensor/pressure/pressure-01/state         (QoS 1, retained)
nightshift/v1/sensor/pressure/pressure-01/telemetry     (QoS 0, not retained)
```

## Payload Contract

### Availability (online)
```json
{
  "schema": "nightshift.sensor-availability.v1",
  "device_id": "pressure-01",
  "online": true,
  "boot_id": "7f92ab31",
  "version": "0.1.0",
  "started_at_ms": 0,
  "time_base": "monotonic_boot_ms"
}
```

### Availability (LWT / offline)
```json
{
  "schema": "nightshift.sensor-availability.v1",
  "device_id": "pressure-01",
  "online": false
}
```

### State
```json
{
  "schema": "nightshift.pressure-state.v1",
  "device_id": "pressure-01",
  "boot_id": "7f92ab31",
  "seq": 123,
  "sampled_at_ms": 123,
  "time_base": "monotonic_boot_ms",
  "gpio": { "4": true, "5": false, "6": true, "7": true },
  "cushion": true,
  "footrest": true,
  "presence": true
}
```

### Telemetry
```json
{
  "schema": "nightshift.pressure-telemetry.v1",
  "device_id": "pressure-01",
  "boot_id": "7f92ab31",
  "uptime_ms": 90000,
  "wifi_rssi_dbm": -51,
  "mqtt_reconnect_count": 1,
  "publish_count": 35,
  "reported_at_ms": 90000,
  "time_base": "monotonic_boot_ms"
}
```

## Build & Flash

The AP may have no internet route, so all `*_at_ms` fields use unsigned
milliseconds since this boot and explicitly declare `monotonic_boot_ms`.
Core reporting never waits for NTP. See [docs/protocol.md](docs/protocol.md)
and the machine-readable examples in [protocol/examples](protocol/examples).

### Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or PlatformIO IDE
- ESP32-S3 connected via USB on **COM9**

### Setup Secrets
```powershell
Copy-Item include/secrets.h.example include/secrets.h
# Edit the ignored include/secrets.h locally.
```

### Build
```powershell
pio run -e esp32-s3-devkitc-1
```

### Flash (COM9, automatic)
```powershell
pio run -e esp32-s3-devkitc-1 --target upload
```

### Serial Monitor
```powershell
pio device monitor -b 115200 -p COM9
```

### Run tests

When a host C++ compiler is installed:

```powershell
pio test -e native
```

The production-linked suite can also run on COM9:

```powershell
pio test -e esp32-s3-tests -f test_core --upload-port COM9 --test-port COM9
```

Target tests temporarily replace the application; flash the normal environment
again afterward. The exact commands used for acceptance are recorded in
[docs/acceptance-test.md](docs/acceptance-test.md).

## Timing

| Parameter | Value |
|---|---|
| GPIO sample interval | 10 ms |
| Press debounce | 30 ms |
| Release debounce | 100 ms |
| State publish (on change) | Immediate |
| State refresh (periodic) | 3 seconds |
| Telemetry interval | 30 seconds |
| MQTT keepalive | 30 seconds |

## Architecture

```
application.cpp       coordinator; sampling stays independent of networking
gpio_sampler.cpp      coherent GPIO register read
debounce.cpp          testable asymmetric debounce and group aggregation
wifi_manager.cpp      DHCP Wi-Fi lifecycle with bounded backoff and jitter
mqtt_manager.cpp      asynchronous ESP-MQTT, LWT, QoS, bounded outbox
payload.cpp           normalized JSON serialization
publish_scheduler.cpp change/reconnect/periodic publish policy
test/test_core/       production-linked Unity tests (host or ESP32 target)
```

## Troubleshooting

| Symptom | Check |
|---|---|
| No serial output | Verify COM9 at 115200 baud and close other serial monitors |
| Flash fails | Close COM9 users and retry automatic reset first; use BOOT only if esptool logs cannot enter download mode |
| Wi-Fi won't connect | Verify SSID/password in secrets.h; check AP is on 2.4GHz ch6 |
| MQTT connect fails | Verify broker on 192.168.51.1:1884; check username/password |
| All logical GPIOs read high | Check wiring; sensors should pull GPIO low when triggered |
| Reconnect loop | Read bounded retry lines in serial; the device should not reboot |

More detail is in [docs/troubleshooting.md](docs/troubleshooting.md).

## MQTT observation

```powershell
# On the Orange Pi or any machine with network access:
mosquitto_sub -h 192.168.51.1 -p 1884 -u pressure-01 -P <password> -t "nightshift/v1/sensor/pressure/#" -v
```

Alternatively set `NIGHTSHIFT_MQTT_PASSWORD` locally and run
`python scripts/observe_mqtt.py`. Neither credentials nor local evidence logs
are tracked.
