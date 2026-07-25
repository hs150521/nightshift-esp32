# Troubleshooting

## COM9 is missing or busy

Run:

```powershell
pio device list
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name
```

Close PlatformIO monitors, terminal programs, and other processes using COM9.
The confirmed adapter is a Silicon Labs CP210x USB-to-UART bridge. Upload uses
automatic DTR/RTS download/reset. Manual BOOT/RESET should only be tried after
esptool explicitly fails to enter download mode.

## Firmware uploads but does not boot

The tested board reports ESP32-S3 revision 0.2, 32 MB octal flash, and 16 MB
octal PSRAM. Keep `board_build.arduino.memory_type = opi_opi` and
`board_build.flash_mode = opi` for this unit. Probe without writing:

```powershell
python "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32s3 --port COM9 flash_id
```

## Wi-Fi does not connect

- Confirm the ignored `include/secrets.h` contains the intended WPA2 password.
- Confirm `stillwork` is broadcasting on 2.4 GHz (currently channel 6).
- Serial logs show each attempt, status code, and bounded retry delay.
- The node uses DHCP; expected addresses are `192.168.51.10`–`.100`.
- Loss of Wi-Fi does not stop sampling and does not cause reboot loops.

Do not interpret an ESP32 `AUTH_EXPIRE` reason as proof of a rejected password.
Before changing firmware or credentials, collect Orange Pi evidence showing:

- a normal phone/client can join `stillwork`;
- hostapd is running on `ap0`;
- `wlan0` and `ap0` are both on channel 6;
- hostapd uses WPA2-PSK with CCMP and does not require PMF;
- hostapd debug output is captured during the ESP32 connection attempt.

Only if a normal client connects while the ESP32 still fails, change one ESP32
diagnostic variable at a time: reverify the ignored secrets, perform one
controlled erase of saved Wi-Fi state, retain `WiFi.persistent(false)`, test
with Wi-Fi sleep disabled, and optionally connect on known channel 6. Preserve
each serial log. Do not erase saved Wi-Fi state on every boot without evidence
that it is necessary.

## MQTT does not connect

- Confirm the broker listens on `192.168.51.1:1884`.
- Confirm the username is `pressure-01` and its ACL allows exactly the three
  pressure topics.
- Confirm the ignored MQTT password is current.
- Watch serial transport errors and reconnect delays. Passwords are never
  logged.

## State does not change

GPIO4–GPIO7 are active-high inputs with internal pull-downs. A press must remain
high for 30 ms; a release must remain low for 100 ms. No 3-second release dwell
exists in this firmware. Never connect a GPIO directly to a voltage above
3.3 V.

## Retained state appears stale

Compare both `boot_id` and `seq`. A new boot intentionally restarts `seq`.
Check `sampled_at_ms` using its `monotonic_boot_ms` time base, not wall-clock
time. On reconnect, the node republishes online availability and a full current
state.
