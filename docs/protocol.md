# Nightshift pressure protocol

The node publishes only these topics:

| Topic suffix | QoS | Retained | Trigger |
|---|---:|---:|---|
| `availability` | 1 | yes | online after every connect; offline LWT |
| `state` | 1 | yes | stable GPIO change, reconnect, and every 3 seconds |
| `telemetry` | 0 | no | approximately every 30 seconds |

The full prefix is
`nightshift/v1/sensor/pressure/pressure-01/`. The firmware has no MQTT
subscriptions and never publishes the Orange Pi authoritative system-state
topic.

## Identity and ordering

- `boot_id` is eight lowercase hexadecimal digits generated from ESP32 hardware
  randomness on every boot.
- `seq` is an unsigned counter incremented for every generated state snapshot.
  It restarts after reboot; consumers must compare `(boot_id, seq)`.
- The MQTT client ID is `pressure-01-` plus six hexadecimal digits derived from
  the eFuse MAC, making it stable and device-specific.

## Time

This installation does not assume internet NTP. `started_at_ms`,
`sampled_at_ms`, and `reported_at_ms` are unsigned milliseconds since boot.
Payloads that contain these fields include:

```json
"time_base": "monotonic_boot_ms"
```

Unsigned subtraction is used for timers, so normal `millis()` rollover does not
break debounce or periodic scheduling.

## State consistency

Serialization recomputes group fields from the four stable GPIO booleans:

```text
cushion = gpio["4"] OR gpio["5"]
footrest = gpio["6"] OR gpio["7"]
presence = cushion OR footrest
```

See [examples](../protocol/examples) for complete payloads. Permanently high or
low input levels are valid and do not generate diagnostic fields.

## Delivery and reconnect behavior

The Espressif MQTT task performs network I/O outside the GPIO sampling loop.
Availability and state use actual MQTT QoS 1; telemetry uses QoS 0. The client
sets its retained QoS-1 LWT before the first connection. Wi-Fi and MQTT use
bounded exponential backoff (1–30 seconds) with jitter.

The application does not accumulate application-level history while offline.
It preserves the latest debounced snapshot and marks a full retained state
pending. On every broker reconnect it immediately queues online availability
and the current full state. The ESP-MQTT outbox is capped by a 2048-byte
application guard; publish failures remain pending and retry at 500 ms without
a busy loop.
