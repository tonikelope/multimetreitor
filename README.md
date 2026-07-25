# ⚡ MULTIMETREITOR

Home electrical consumption monitor based on the **ESP8266** and the **PZEM-004T v3.0** energy meter. It measures the voltage, current, power, energy, frequency and power factor of your mains installation, shows them on an LCD display and publishes them over **MQTT**. It also includes a **thermal model of the ICP** (the main circuit breaker used in Spain to enforce your contracted power) that warns you *before* the utility cuts your power for drawing too much.

It ships with a **web configuration panel** served by the device itself and a **Rainmeter skin** to display the metrics on the Windows desktop.

---

## ✨ Features

- 📊 **Real-time measurement** via the PZEM-004T v3: voltage (V), current (A), power (W), energy (kWh), frequency (Hz) and power factor.
- 🔥 **ICP thermal model**: a first-order thermal-image model (IEC 60255-149) of the main breaker's bimetal that tracks how much of its trip time is already used up, driven by a single sensitivity selector set to the worst case by default. It warns you *before* the utility cuts your power for drawing too much. See [ICP thermal model](#-icp-thermal-model).
- 🚨 **Configurable alerts**: ICP, overvoltage, undervoltage and consumption (by amperes or watts), with an optional **buzzer**.
- 🖥️ **16x2 I2C LCD** with metrics selectable through a bitmask, plus WiFi and MQTT status indicators.
- 🌐 **Responsive web panel** (HTML embedded in `PROGMEM`) to configure everything without recompiling.
- 🌍 **Bilingual UI (Spanish and English)** in both the web panel and the Rainmeter skin, with a one-click toggle. The default is Spanish.
- 📡 **MQTT publishing** with a unified JSON payload and *retained* messages.
- 🗓️ **Monthly consumption history** (24 months) with automatic energy reset on month change, persisted to EEPROM.
- 🕐 **NTP synchronization** with the Spanish mainland timezone (CET and CEST, with automatic DST changes).
- 🔄 **OTA updates** (Over-The-Air), password protected.
- 💾 **EEPROM persistence** with integrity validation (magic plus version) and wear protection (a one-hour write cooldown for automatic tasks).

<div align="center">
  <img src="docs/web_ui_en_mobile.png" alt="MULTIMETREITOR web configuration panel (mobile, English)" width="300">
  <br>
  <em>Web configuration panel, mobile view (English)</em>
</div>

---

## 🔌 Hardware

| Component             | Detail                                    |
|-----------------------|-------------------------------------------|
| MCU                   | ESP8266 (NodeMCU / Wemos D1 mini)         |
| Meter                 | PZEM-004T v3.0 (UART)                      |
| Display               | 16x2 LCD with I2C backpack (address `0x27`) |
| Alarm                 | Active buzzer                             |

### Wiring (pins)

| Signal            | ESP8266 pin | Notes                          |
|-------------------|-------------|--------------------------------|
| PZEM RX           | `D6`        | `SoftwareSerial` RX            |
| PZEM TX           | `D5`        | `SoftwareSerial` TX            |
| LCD SDA / SCL     | `D2` / `D1` | Default I2C (`Wire`)           |
| Buzzer            | `D7`        | Digital output                 |

![Wiring diagram](docs/multimetreitor_wiring.png)

---

## 🧩 Firmware architecture

![Firmware architecture](docs/multimetreitor_architecture.png)

**Monolithic** firmware (`multimetreitor.ino`) organized into functional blocks:

- **Config / EEPROM**: `struct AppConfig` serialized to EEPROM with validation and default values.
- **WiFi / OTA / NTP**: connection with retries, OTA and time sync via a POSIX TZ string.
- **MQTT**: publishes state, log and status, and recovers the ICP state at boot (it reads the *retained* message from its own topic).
- **ICP model** (`computeICP`): integrates the breaker's thermal state from the I/In ratio with a first-order thermal-image model (IEC 60255-149). See [ICP thermal model](#-icp-thermal-model).
- **Alerts** (`evaluateAlerts`): evaluated in priority order, highest first: ICP, then overvoltage, then undervoltage, then consumption.
- **LCD** (`composeLCDLines`): composes two 16-character lines with the active metrics.
- **Web server**: configuration panel and JSON endpoints.
- **History**: monthly consumption management and month-change handling.

### MQTT topics

| Topic                              | Direction  | Content                                          |
|------------------------------------|------------|--------------------------------------------------|
| `electricidad/casa/estado`         | publish    | JSON with all metrics and active alerts (*retained*) |
| `electricidad/casa/icp`            | subscribe  | ICP state recovery at boot                       |
| `electricidad/casa/alertas_config` | publish    | Alert configuration: enabled flags and thresholds (*retained*, refreshed on connect and on config save) |
| `multimetreitor/status`            | publish    | `online` (*retained*)                            |
| `multimetreitor/serial`            | publish    | Serial-port log                                  |

**Example state payload:**

```json
{
  "voltaje": "230.5V",
  "corriente": "12.34A",
  "potencia": "2840W",
  "energia": "123.45kWh",
  "factor_potencia": "0.95",
  "frecuencia": "50.0Hz",
  "icp": "62%",
  "alerts": ["sobretension"],
  "timestamp": 1700000000
}
```

> ℹ️ The JSON keys are in Spanish (`voltaje`, `corriente`, and so on) because they are the published data contract consumed by the Rainmeter skin.

`alerts` lists the currently active alerts (`[]` when none): `icp`, `sobretension`, `subtension`, `consumo`. The same array is served by the web endpoint `/json`. To interpret it, external apps can read the alert configuration (the enabled checkbox and the configured threshold per alert) from the retained `electricidad/casa/alertas_config` topic or from `/json_alerts`:

```json
{
  "sobretension": { "enabled": true,  "umbral": 250.0, "unidad": "V" },
  "subtension":   { "enabled": true,  "umbral": 200.0, "unidad": "V" },
  "consumo":      { "enabled": false, "umbral": 0.00,  "unidad": "W" },
  "icp":          { "enabled": true,  "nominal": 25.00, "umbral": 40, "unidad": "%" }
}
```

---

## 🔥 ICP thermal model

In Spain the **ICP** (*Interruptor de Control de Potencia*) is the main breaker the utility uses to enforce your contracted power. Draw more current than your tariff allows for long enough and it trips, leaving the house dark. It is a **thermal-magnetic** device: a bimetal strip that bends as it heats (for slow, inverse-time overloads) plus a magnetic coil for instantaneous short-circuit trips. MULTIMETREITOR models the **thermal** half so it can warn you *seconds before* the bimetal lets go, while you can still go and switch something off.

### The reference curve

A thermal-magnetic breaker does not trip at a fixed time. The maker only guarantees a **band** that accounts for manufacturing tolerance and ambient temperature. The model is fitted to the slow and fast edges of this **UNE 20317** magneto-thermal band:

<div align="center">
  <img src="docs/une20317.png" alt="UNE 20317 magneto-thermal trip band" width="480">
  <br>
  <em>UNE 20317 trip band. The hatched area is the tolerance band. The coloured verticals at 1.2, 1.5 and 2 times In mark where the fast (lower) and slow (upper) edges cross. For example, at 2 times In the breaker may trip anywhere from about 10 s to about 2 min. The steep fall at 5 to 8 times In is the magnetic (instantaneous) region, which is not modelled here.</em>
</div>

The band is enormous, roughly a **factor of 80 in time** at a given current, so no single curve is truly the right one. Rather than pick a middle guess, MULTIMETREITOR lets you slide across the band and **defaults to the worst (fastest) edge** (see the sensitivity selector below).

### The algorithm

Instead of a lookup table, the firmware runs the standard **first-order thermal image** of **IEC 60255-149**, the same model that real numerical protection relays use. It keeps one state variable, `H`, the normalized heat of the bimetal, where `H = 1.0` (100%) is the trip itself. Every cycle it relaxes `H` towards the equilibrium heat for the current flowing right now:

```
H = Heq + (H - Heq) * exp(-dt / tau)        Heq = (I / In)^2 / k^2
```

- **`I / In`**: the measured current divided by the configured nominal (contracted) current.
- **`k`**: the trip threshold as a multiple of In. Below `k` the equilibrium `Heq` stays at 1 or under, so `H` can never reach the trip and **the breaker holds forever**. Default `k = 1.07`.
- **`tau`**: the thermal time constant, that is, how fast the bimetal heats and cools. Default `tau = 384 s`.
- The **square** in `Heq` comes from Joule's law: heating is proportional to the square of the current.

From `H` and the present current, the model solves the same equation forward to get the number that actually matters, the **seconds left before the trip**:

```
t = tau * ln((Heq - H) / (Heq - 1))         valid while Heq > 1, that is while I > k * In
```

These are `computeICP()` (the integrator) and `icpSegundosRestantes()` (the forward solve) in `multimetreitor.ino`.

### Sensitivity selector (worst case by default)

The whole tolerance band sits behind **one** control, the *ICP sensitivity* slider (0 to 100%), so there are no cryptic parameters to tune. It leaves `k` and `tau` untouched, since changing them would make the reading twitchy, and instead sets a **preheat floor**, the minimum thermal state the bimetal is assumed to have started this overload from:

```
floor = (sensitivity / 100) * FLOOR_MAX         FLOOR_MAX = 0.922
```

The countdown then assumes `H` is at least `floor`, but the **real integrated heat still wins whenever it is higher**, so an already-hot breaker is never underestimated:

- **100%, the worst case and the default.** With `floor = 0.922` the breaker is assumed to be nearly preheated, which is the **fast edge** of the band and gives the shortest, most cautious time to trip. It is the default because a false alarm is a minor annoyance, whereas a missed trip is a dark house.
- **0%, the slow case.** With `floor = 0` the breaker starts cold, which is the **slow edge** of the band and gives the latest possible warning.

At the worst-case default (with In = 25 A) the model reproduces the fast edge of the reference image:

| I / In | Current | Trip time (worst case) |
|:------:|:-------:|:----------------------:|
| 1.20   | 30.0 A  | about 1.7 min |
| 1.45   | 36.3 A  | about 34 s |
| 2.00   | 50.0 A  | about 12 s |
| 3.00   | 75.0 A  | about 4.3 s |

### Cooling

Cooling is not modelled as a separate rule. It is the very same equation relaxing towards a *lower* equilibrium. When the current drops, `Heq` falls (it follows the square of the current), so `H` decays **exponentially** towards it, all the way down to 0 with no load. This replaces the old linear behaviour that went from 100% to 0% over a fixed number of seconds.

The de-energized cooling constant is `tau2`, used when the breaker draws essentially nothing (below 5% of In, meaning the house is off, the mains are down or the breaker has already tripped). For a **passive bimetal, cooling is as slow as heating**, so `tau2 = tau = 384 s` by design. The two are kept as separate fields only because the standard allows them to differ. After a reboot, the elapsed offline cooling is applied in closed form, `H = H_stored * exp(-t_off / tau2)`. If there is no valid NTP clock the cooling is skipped entirely, which is the conservative choice: keep the last known heat rather than assume it cooled.

### The danger bar and the warning

What you see on the LCD, web panel, MQTT and Rainmeter is a **countdown bar** rather than the raw heat. It reads *how much of your reaction time is already gone*, as a percentage of a configurable **warning window** (120 s by default):

```
bar = 100 * (1 - t_left / window)       it is 0 when t_left >= window, or when this load can never trip
```

Shown this way, the percentage means the same thing at every current. With a 120 s window, 50% is always 60 seconds left, whereas the raw heat would be ten seconds at 64 A and six minutes at 33 A. The alert fires, and the buzzer sounds, when the bar reaches the **warning threshold**, that is when

```
t_left <= window * (1 - threshold / 100)
```

For example, a 120 s window with a 40% threshold warns you when **72 s or less** remain before the trip, which is `120 * (1 - 0.40)`. An alert must persist for `ALERT_TRIGGER_SAMPLES = 3` consecutive readings before it latches, so with the PZEM's averaging of roughly 1.3 s the confirmed warning lands a few seconds after the overload actually begins.

<div align="center">
  <img src="docs/icp.png" alt="ICP thermal alert configuration panel" width="360">
  <br>
  <em>ICP panel in the web config: nominal current, warning window, warning threshold, the sensitivity selector (shown at worst case) and the resulting trip table.</em>
</div>

### Persistence

The thermal state survives a reboot. `computeICP()` publishes `H` (with a timestamp) to a *retained* MQTT topic, and at boot the device reads it back, applies the elapsed cooling and resumes, so a breaker that was hot before a brief power blip is not treated as cold. Real overload episodes (and probable trips) are also written to a small on-device forensic ring buffer and published over MQTT.

> ℹ️ **Calibration.** The factory band is so wide that no theoretical curve is exactly right for one physical breaker. The defaults (`k = 1.07`, `tau = 384 s`, worst-case sensitivity) follow the UNE 20317 image above and deliberately err on the early side. The episode log exists so the model can later be refined against **real trips** rather than nameplate figures.

---

## 🖥️ Rainmeter skin

`Rainmeter/Multimetreitor/` contains a skin that displays the MULTIMETREITOR metrics directly on the Windows desktop, consuming the data over MQTT.

**Skin features:**

- Reads from the MQTT broker through Rainmeter's **[MqttClient](https://github.com/anschnapp/MqttPlugin)** plugin and parses the JSON published by the firmware.
- Shows: **Voltage, Frequency, Current, ICP (progress bar), Power, Power Factor and monthly Consumption**.
- **Visual warnings**: a red background on *Current* when it exceeds 30 A, and an ICP bar proportional to the thermal load (width is `ICP * 2.5`).
- Automatically fixes locale decimals (comma to dot) and the connection status for internal calculations (`Substitute`).
- Includes optional support for a **water heater** (`calentador_estado`, `calentador_corriente`) that lights up red when it is off.
- **Bilingual labels (ES and EN)**: click the language button (top-right of the skin) to switch. The choice is saved in the `Language` variable (see [Languages](#-languages-es--en)).

### Installing the skin

1. Install [Rainmeter](https://www.rainmeter.net/).
2. Install the **MqttClient** plugin (copy the `.dll` into `Rainmeter/Plugins`).
3. Copy the `Rainmeter/Multimetreitor` folder into `Documents/Rainmeter/Skins/`.
4. Edit the `[Variables]` section of the `.ini` and set:
   - `MQTT_BROKER`: your MQTT broker IP.
   - `MQTT_TOPIC`: the firmware's state topic (`electricidad/casa/estado`).
5. Load the skin from Rainmeter (*Refresh all* or *Manage*).

> ℹ️ By default the `.ini` ships with `MQTT_TOPIC=rainmeter/multimetreitor`. Change it to the topic the firmware actually publishes (`electricidad/casa/estado`), or adapt the topic on the device.

---

## 🌍 Languages (ES / EN)

Both UIs are bilingual and **default to Spanish**. Only the labels and UI text are translated. The metric values and units (V, A, W, and so on) are language-neutral.

### Web panel
- Click the **`EN` / `ES` button** at the top-right of the page to switch language instantly (client-side, no reload).
- The choice is remembered per browser via `localStorage` (`mmt_lang`).
- To add or tweak strings, edit the `I18N = { es: {...}, en: {...} }` dictionary in the embedded `<script>` of `multimetreitor.ino`. Translatable elements are marked with `data-i18n="key"`.

### Rainmeter skin
- Click the **language button** at the top-right of the skin to toggle between ES and EN. The choice is persisted to the `Language` variable in the `.ini` (`!WriteKeyValue` and `!Refresh`).
- This uses Rainmeter's standard localization pattern: a `Language` variable in `[Variables]` plus `@Include=#@#Lang_#Language#.inc`.
- Translations live in `Rainmeter/Multimetreitor/@Resources/Lang_ES.inc` and `Lang_EN.inc`. To change the default, set `Language=ES` (or `EN`) in `[Variables]`.

---

## 🛠️ Build and flash

**Requirements (Arduino IDE or arduino-cli):**

- The **ESP8266** Arduino core.
- Libraries: `PubSubClient`, `LiquidCrystal_I2C`, `PZEM004Tv30`, `ArduinoJson`, `ESP8266WebServer`, `ArduinoOTA`, `EspSoftwareSerial`.

**Steps:**

1. **Create your `secrets.h`** (see below) with your WiFi credentials.
2. Open `multimetreitor.ino` in the Arduino IDE.
3. Select the matching ESP8266 board.
4. Adjust the static IP and network configuration in `multimetreitor.ino` if needed.
5. Upload over USB the first time. After that you can update over **OTA** (hostname `multimetreitor-ota`).

### 🔐 Credentials (`secrets.h`)

WiFi credentials are kept **out of the source tree** in a `secrets.h` file that is git-ignored, so they are never committed. The repo ships a template, `secrets.h.example`.

To configure after cloning:

```bash
cp secrets.h.example secrets.h
```

Then edit `secrets.h` with your own values:

```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
```

`multimetreitor.ino` includes it via `#include "secrets.h"` and uses `WIFI_SSID` and `WIFI_PASSWORD`. The WiFi password is also reused as the OTA password.

> ℹ️ The static IP (`192.168.1.24`), gateway and hostnames are configured directly in `multimetreitor.ino`. Adjust them to your network.

---

## 🏠 Network note

MULTIMETREITOR is designed as a **local home-network appliance**. The web panel keeps things fast and simple and is meant to live inside your own trusted Wi-Fi. Because of that, **run it on a secure, trusted network and do not expose it directly to the Internet** (no port-forwarding). That is the intended setup. For multi-user or untrusted environments you can add HTTP Basic Auth to the control endpoints.

---

## 📝 License

Licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE).

Copyright (c) tonikelope
