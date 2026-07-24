# Nightshift ESP32 Pressure Node

Four-input digital pressure sensor node for the Nightshift system.  
**Device ID:** `pressure-01` | **Board:** ESP32-S3-DevKitC-1

## Hardware Mapping

| ESP32-S3 GPIO | Meaning | Active Level |
|---|---|---|
| GPIO4 | Cushion left | High = triggered |
| GPIO5 | Cushion right | High = triggered |
| GPIO6 | Footrest left | High = triggered |
| GPIO7 | Footrest right | High = triggered |

**Logical Groups:**
- `cushion = GPIO4 OR GPIO5`
- `footrest = GPIO6 OR GPIO7`
- `presence = cushion OR footrest`

Pins are configured with **internal pull-down** resistors (active-high inputs).

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
| Client ID | `pressure-01-<boot_id>` |
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
  "started_at_ms": 1780000000000
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
  "sampled_at_ms": 1780000000123,
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
  "reported_at_ms": 90000
}
```

## Build & Flash

### Prerequisites
- [PlatformIO CLI](https://platformio.org/install/cli) or PlatformIO IDE
- ESP32-S3 connected via USB on **COM9**

### Setup Secrets
```bash
cp include/secrets.h.example include/secrets.h
# Edit include/secrets.h with real Wi-Fi and MQTT credentials
```

### Build
```bash
pio run -e esp32-s3-devkitc-1
```

### Flash (COM9, automatic)
```bash
pio run -e esp32-s3-devkitc-1 --target upload
```

### Serial Monitor
```bash
pio device monitor -b 115200 -p COM9
```

### Run Tests (native host)
```bash
pio test -e native
```

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
src/
├── main.cpp          # Application coordinator
├── debounce.cpp      # GPIO sampling + asymmetric debounce
├── wifi_manager.cpp  # Wi-Fi STA with exponential backoff
├── mqtt_manager.cpp  # MQTT lifecycle, LWT, reconnect
└── payload.cpp       # ArduinoJson serialization
include/
├── config.h          # Pin mapping, timing, topics
├── debounce.h        # Debounce interface
├── wifi_manager.h    # Wi-Fi interface
├── mqtt_manager.h    # MQTT interface
├── payload.h         # Payload builder interface
├── secrets.h         # (gitignored) Real credentials
└── secrets.h.example # Template for credentials
test/
└── test_native/      # Native unit tests (Unity)
```

## Troubleshooting

| Symptom | Check |
|---|---|
| No serial output | Verify COM9, 115200 baud, USB CDC mode |
| Flash fails | Try holding BOOT while plugging USB; release after upload starts |
| Wi-Fi won't connect | Verify SSID/password in secrets.h; check AP is on 2.4GHz ch6 |
| MQTT connect fails | Verify broker on 192.168.51.1:1884; check username/password |
| All GPIOs read low | Check wiring; sensors should pull GPIO high when triggered |
| Watchdog resets | Check for blocking code in loop(); delay(1) yields to RTOS |

## MQTT Observation (mosquitto_sub)

```bash
# On the Orange Pi or any machine with network access:
mosquitto_sub -h 192.168.51.1 -p 1884 -u pressure-01 -P <password> -t "nightshift/v1/sensor/pressure/#" -v
```
