# BH1750 Sensor Provider

A [Sensor Hub](../sensor-hub/readme.md) provider usermod for the Rohm
BH1750 ambient light sensor - registers `bh1750_illuminance` (lx) with the
hub by default, which then handles MQTT, Home Assistant discovery, the
JSON API and the Info tab. Runs in continuous high-resolution mode.

## Hardware

Wire SDA/SCL to the I2C pins configured on WLED's own **Config > LED
Preferences** page (shared across all I2C usermods). This usermod does not
call `Wire.begin()` itself. Both common addresses (`0x23` with ADDR low,
`0x5C` with ADDR high) are probed automatically. Retries `begin()` every
10s if the sensor isn't found; after 3 consecutive failed reads the sensor
is marked unavailable in Home Assistant, after 10 it re-attempts
`begin()`.

## Usage

Self-contained out-of-tree usermod (see `library.json` for its
`claws/BH1750` dependency). Add it to `custom_usermods` next to the
[Sensor Hub](../sensor-hub/readme.md) itself.

## Usermod Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | on | Master on/off switch (also auto-disabled if I2C pins aren't configured) |
| Check interval | 10s | How often the sensor is read |
| Name prefix | `bh1750` | Sensor name becomes `<prefix>_illuminance` - must be unique across every provider registered with the hub |
| Precision | 0 | Decimal places published |
