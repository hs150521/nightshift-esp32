"""Observe only pressure-01 topics without storing credentials.

Requires: python -m pip install paho-mqtt
Set NIGHTSHIFT_MQTT_PASSWORD in the local environment before running.
"""

from __future__ import annotations

import json
import os
import sys
from datetime import datetime, timezone

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Install paho-mqtt: python -m pip install paho-mqtt", file=sys.stderr)
    raise SystemExit(2)

HOST = "192.168.51.1"
PORT = 1884
USERNAME = "pressure-01"
PREFIX = "nightshift/v1/sensor/pressure/pressure-01/"
ALLOWED = {
    PREFIX + "availability",
    PREFIX + "state",
    PREFIX + "telemetry",
}


def on_connect(client: mqtt.Client, _userdata, _flags, reason_code, _properties):
    if reason_code != 0:
        print(f"MQTT connection failed: {reason_code}", file=sys.stderr)
        return
    client.subscribe(PREFIX + "#", qos=1)
    print(f"connected to {HOST}:{PORT}; observing {PREFIX}#")


def on_message(_client: mqtt.Client, _userdata, message: mqtt.MQTTMessage):
    if message.topic not in ALLOWED:
        print(f"rejected unexpected topic: {message.topic}", file=sys.stderr)
        return
    timestamp = datetime.now(timezone.utc).isoformat()
    try:
        payload = json.loads(message.payload)
        encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        encoded = repr(message.payload)
    print(
        f"{timestamp} topic={message.topic} qos={message.qos} "
        f"retain={message.retain} payload={encoded}",
        flush=True,
    )


def main() -> int:
    password = os.environ.get("NIGHTSHIFT_MQTT_PASSWORD")
    if not password:
        print("Set NIGHTSHIFT_MQTT_PASSWORD locally.", file=sys.stderr)
        return 2
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="")
    client.username_pw_set(USERNAME, password)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(HOST, PORT, keepalive=30)
    client.loop_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
