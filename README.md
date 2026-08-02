# ⚡ MULTIMETREITOR

<div align="justify">

Home electrical consumption monitor based on the **ESP8266** and the **PZEM-004T v3.0** energy meter. It measures the voltage, current, power, energy, frequency and power factor of your mains installation, shows them on an LCD display and publishes them over **MQTT**. It also includes a **thermal model of the ICP** (the main circuit breaker used in Spain to enforce your contracted power) that warns you *before* the utility cuts your power for drawing too much.

Beyond monitoring, a built-in **rules engine** can fire actions (an MQTT publish or a webhook) when your own conditions on the measurements are met, so the device can act on its own, for example by shedding a load such as a water heater before the ICP trips.

It ships with a **web configuration panel** served by the device itself and a **Rainmeter skin** to display the metrics on the Windows desktop.

</div>

---

## ✨ Features

<div align="justify">

- 📊 **Real-time measurement** via the PZEM-004T v3: voltage (V), current (A), power (W), energy (kWh), frequency (Hz) and power factor.
- 🔥 **ICP thermal model**: a first-order thermal-image model (IEC 60255-149) of the main breaker's bimetal, integrated continuously from the measured current, shown as a bar that reads empty in ordinary use and fills as the breaker approaches its trip. It warns you *before* the utility cuts your power for drawing too much. See [ICP thermal model](#-icp-thermal-model).
- 🚨 **Configurable alerts**: ICP, overvoltage, undervoltage and consumption (by amperes or watts), with an optional **buzzer**.
- ⚙️ **Rules / Triggers**: up to 16 user-defined rules (up to 8 conditions and 4 actions each) that publish an MQTT message or call a webhook when your conditions on the live measurements (current, voltage, power, ICP load, and so on) are met, with AND/OR logic and anti-bounce persistence. Enough to shed a load before the ICP trips. See [Rules / Triggers](#-rules--triggers).
- 🧾 **ICP forensic log**: every overload episode — and any *real* trip — is recorded with its timestamp, duration, peak current and peak danger level, viewable on a sortable web page with in-place, configurable record thresholds.
- 🖥️ **16x2 I2C LCD** with metrics selectable through a bitmask, plus WiFi and MQTT status indicators.
- 🌐 **Responsive web panel** (HTML embedded in `PROGMEM`) to configure everything without recompiling.
- 🌍 **Bilingual UI (Spanish and English)** in both the web panel and the Rainmeter skin, with a one-click toggle. The default is Spanish.
- 📡 **MQTT publishing** with a unified JSON payload and *retained* messages.
- 🗓️ **Consumption history** — **monthly (unlimited), daily and hourly**, with automatic energy reset on month change. Shown on a web page with bar charts and a **per-day hourly drill-down** (tap a day or pick a date). See [Consumption history](#-consumption-history).
- 🕐 **NTP synchronization** with the Spanish mainland timezone (CET and CEST, with automatic DST changes).
- 🔄 **OTA updates** (Over-The-Air), password protected.
- 💾 **Storage** — small config/calibration in EEPROM (integrity-validated: magic + version, with a one-hour write cooldown for wear); everything that grows (forensic log, energy history, rules) lives in the **~2 MB LittleFS flash filesystem**, so it is effectively unlimited.

</div>

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

<div align="justify">

**Monolithic** firmware (`multimetreitor.ino`) organized into functional blocks:

- **Config / EEPROM**: `struct AppConfig` serialized to EEPROM with validation and default values.
- **WiFi / OTA / NTP**: connection with retries, OTA and time sync via a POSIX TZ string.
- **MQTT**: publishes state, log and status, and recovers the ICP state at boot (it reads the *retained* message from its own topic). The ICP protection itself does not depend on any of this — see [Persistence](#persistence).
- **ICP model** (`computeICP`): integrates the breaker's thermal state from the I/In ratio with a first-order thermal-image model (IEC 60255-149). See [ICP thermal model](#-icp-thermal-model).
- **Alerts** (`evaluateAlerts`): evaluated in priority order, highest first: ICP, then overvoltage, then undervoltage, then consumption.
- **Rules engine** (`evaluateRules`): user-defined triggers stored in EEPROM and evaluated every cycle. Each rule fires an MQTT publish or a webhook on the activate and clear edges. See [Rules / Triggers](#-rules--triggers).
- **LCD** (`composeLCDLines`): composes two 16-character lines with the active metrics.
- **Web server**: configuration panel and JSON endpoints.
- **History**: monthly consumption management and month-change handling.

</div>

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

<div align="justify">

`alerts` lists the currently active alerts (`[]` when none): `icp`, `sobretension`, `subtension`, `consumo`. The same array is served by the web endpoint `/json`. To interpret it, external apps can read the alert configuration (the enabled checkbox and the configured threshold per alert) from the retained `electricidad/casa/alertas_config` topic or from `/json_alerts`:

</div>

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

<div align="justify">

In Spain the **ICP** (*Interruptor de Control de Potencia*) is the main breaker the utility uses to enforce your contracted power. Draw more current than your tariff allows for long enough and it trips, leaving the house dark. It is a **thermal-magnetic** device: a bimetal strip that bends as it heats (for slow, inverse-time overloads) plus a magnetic coil for instantaneous short-circuit trips. MULTIMETREITOR models the **thermal** half so it can warn you *seconds before* the bimetal lets go, while you can still go and switch something off.

This models a **physical thermal-magnetic ICP** (the Merlin Gerin ICP-M type). If your contracted power is enforced by the **electronic meter** instead (*telegestión* / *modo maxímetro*, with no physical breaker doing the cut-off in your panel), the trip behaviour is set by the meter's firmware and this model does not describe it, though as a conservative early warning it is still useful.

</div>

### The reference curve

<div align="justify">

A thermal-magnetic breaker does not trip at a fixed time. The maker only guarantees a **band** that accounts for manufacturing tolerance and ambient temperature. The model is fitted to the slow and fast edges of this **UNE 20317** magneto-thermal band:

</div>

<div align="center">
  <img src="docs/une20317.png" alt="UNE 20317 magneto-thermal trip band" width="480">
  <br>
  <em>UNE 20317 trip band. The hatched area is the tolerance band. The coloured verticals at 1.2, 1.5 and 2 times In mark where the fast (lower) and slow (upper) edges cross. For example, at 2 times In the breaker may trip anywhere from about 10 s to about 2 min. The steep fall at 5 to 8 times In is the magnetic (instantaneous) region, which is not modelled here.</em>
</div>

<div align="justify">

The band is enormous, roughly a **factor of 80 in time** at a given current, so no single curve is truly the right one. Rather than pick a middle guess, MULTIMETREITOR lets you slide across the band and **defaults to the worst (fastest) edge** (see the sensitivity selector below).

</div>

### The algorithm

<div align="justify">

Instead of a lookup table, the firmware runs the standard **first-order thermal image** of **IEC 60255-149**, the same model that real numerical protection relays use. It keeps one state variable, `H`, the normalized heat of the bimetal, where `H = 1.0` (100%) is the trip itself. Every cycle it relaxes `H` towards the equilibrium heat for the current flowing right now:

</div>

```
H = Heq + (H - Heq) * exp(-dt / tau)        Heq = (I / In)^2 / k^2
```

<div align="justify">

- **`I / In`**: the measured current divided by the configured nominal (contracted) current.
- **`k`**: the trip threshold as a multiple of In. Below `k` the equilibrium `Heq` stays at 1 or under, so `H` can never reach the trip and **the breaker holds forever**. Default `k = 1.07`.
- **`tau`**: the thermal time constant, that is, how fast the bimetal heats and cools. Default `tau = 384 s`.
- The **square** in `Heq` comes from Joule's law: heating is proportional to the square of the current.

From `H` and the present current, the model solves the same equation forward to get the number that actually matters, the **seconds left before the trip**:

</div>

```
t = tau * ln((Heq - H) / (Heq - 1))         valid while Heq > 1, that is while I > k * In
```

<div align="justify">

These are `computeICP()` (the integrator, which also drives the bar), `icpNivelPeligro()` (the bar) and `icpSegundosRestantes()` (the forward solve) in `multimetreitor.ino`.

</div>

### Sensitivity selector (worst case by default)

<div align="justify">

The whole tolerance band sits behind **one** control, the *ICP sensitivity* slider (0 to 100%), so there are no cryptic parameters to tune. It leaves `k` and `tau` untouched, since changing them would make the reading twitchy, and instead sets a **preheat floor**, the thermal state the bimetal is assumed to be in when the device has no way of knowing it:

</div>

```
floor = (sensitivity / 100) * FLOOR_MAX         FLOOR_MAX = 0.922
```

<div align="justify">

- **100%, the worst case and the default.** With `floor = 0.922` the breaker is assumed to be nearly preheated, which is the **fast edge** of the band and gives the shortest, most cautious time to trip. It is the default because a false alarm is a minor annoyance, whereas a missed trip is a dark house.
- **0%, the slow case.** With `floor = 0` the breaker starts cold, which is the **slow edge** of the band and gives the latest possible warning.

That floor applies where the thermal state is genuinely unknown, which is **at boot**: it caps the seed the device starts from (see [Persistence](#persistence)). Once the model has been integrating measured current there is a real thermal history, and that history is what the bar, the countdown and the alert use. An earlier version applied the floor to the countdown at all times, which sounds conservative but quietly destroyed the integrator's whole purpose: with a floor of 0.922 and a house sitting at a perfectly normal 14% to 56% of trip heat, the assumed value always won, and the reading became a function of the instantaneous current with no memory of the preceding hour at all.

At the worst-case boot assumption (with In = 25 A) the model reproduces the fast edge of the reference image:

</div>

| I / In | Current | Trip time (worst case) |
|:------:|:-------:|:----------------------:|
| 1.20   | 30.0 A  | about 1.7 min |
| 1.45   | 36.3 A  | about 34 s |
| 2.00   | 50.0 A  | about 12 s |
| 3.00   | 75.0 A  | about 4.3 s |

### Cooling

<div align="justify">

Cooling is not modelled as a separate rule. It is the very same equation relaxing towards a *lower* equilibrium. When the current drops, `Heq` falls (it follows the square of the current), so `H` decays **exponentially** towards it, all the way down to 0 with no load. This replaces the old linear behaviour that went from 100% to 0% over a fixed number of seconds.

The de-energized cooling constant is `tau2`, used when the breaker draws essentially nothing (below 5% of In, meaning the house is off, the mains are down or the breaker has already tripped). De-energized cooling has no internal heat source, only convection to ambient, so it is **slower** than energized heating: `tau2` defaults to `576 s`, about **1.5 times** `tau`, deliberately erring on the side of retaining residual heat after a trip. They are kept as separate fields because the standard allows them to differ, and `tau2` can be retuned from a real logged trip. After a reboot, the elapsed offline cooling is applied in closed form, `H = H_stored * exp(-t_off / tau2)`. If there is no valid NTP clock the cooling is skipped entirely, which is the conservative choice: keep the last known heat rather than assume it cooled.

</div>

### The danger bar and the warning

<div align="justify">

What you see on the LCD, web panel, MQTT and Rainmeter is the **warning window counted down to the trip**. The model gives the seconds left directly, by solving the thermal image forward at the present current from the heat it has actually integrated:

</div>

```
t_left = tau * ln( (Heq - H) / (Heq - 1) )      defined while Heq > 1
bar    = 100 * (1 - t_left / window)            clamped to 0..100
```

<div align="justify">

So the bar is empty while the trip is further away than the **warning window** (120 s by default), full at the trip, and linear in between: **half full means half the window left, at any current**. Below `k*In` there is no trip to count down to and it reads zero, which is why an ordinary house never sees a bar however much it draws. That linearity is the whole point — the bar has to be readable at a glance from across the room, when the buzzer is too far away to hear.

The bar may rise instantly, because a warning must never arrive late, but it may only **fall at the bimetal's own cooling rate**. Following the instantaneous estimate downwards is what made an earlier version flash: the time-to-trip estimate has a pole at `I = k*In`, so a load cycling between 30 A and 27 A swung the bar the full 0–100 % every few seconds, ~80 points in a single cycle. Braking the descent keeps the number honest — after a surge the breaker really is still hot — and means that easing the load reads as the breaker cooling down over the following half hour rather than as the indicator vanishing.

It is deliberately **not** the thermal level `H` rescaled. Those two cannot be the same number: a rescaled temperature has to put its zero at or above `1/k²`, the equilibrium of drawing exactly the contracted current, or it would never read empty during ordinary use — and a violent overload crosses that level well inside the window. At 1.6x In a rescaled-temperature bar left zero with 37 s to go instead of the 120 s configured. `icpCarga` remains the raw thermal level and is what the model itself integrates; the bar is the clock built on top of it.

This gives **two staged warnings**. The bar leaving zero is the silent visual one, at the full window. The **warning threshold** is where the buzzer, the LCD banner and the MQTT flag join in — and because the bar is linear in the time left, that threshold reads directly as a margin, with no dependence on the current or on `tau`:

</div>

```
buzzer starts with   window * (1 - threshold/100)   seconds left
```

<div align="justify">

40 % of a 120 s window is 72 s. Beeping for the whole window would be unbearable and would throw the quiet stage away. The trigger also requires a load that can still trip the breaker, which is what stops it latching for ever on something like a steady 26 A: that sits at 94 % of the trip level and the breaker sustains it indefinitely, so it is worth showing on the bar but is no emergency. An alert must persist for `ALERT_TRIGGER_SAMPLES = 3` consecutive readings before it latches, which is where the couple of seconds of slippage in the table below comes from.

Measured from a house idling at 6.35 A, with the defaults and a 40 % threshold:

</div>

| Overload | Trips in | Bar leaves 0 | Buzzer |
|:---:|:---:|:---:|:---:|
| 28 A | 917 s | 120 s before | 70 s before |
| 30 A | 592 s | 120 s before | 70 s before |
| 35 A | 325 s | 120 s before | 70 s before |
| 40 A | 218 s | 120 s before | 70 s before |
| 50 A | 124 s | 120 s before | 70 s before |
| 60 A | 81 s | 80 s before | 70 s before |

<div align="justify">

The threshold holds its promise across the whole range — 10 % gives 106 s, 25 % gives 88 s, 50 % gives 58 s, 75 % gives 28 s. Only past about 2.4x In does physics cap it instead of the setting: the breaker trips in less than the window itself, so there is no two minutes left to give.

</div>

<div align="center">
  <img src="docs/icp.png" alt="ICP thermal alert configuration panel" width="360">
  <br>
  <em>ICP panel in the web config: nominal current, warning window, warning threshold, the sensitivity selector (shown at worst case) and the resulting trip table.</em>
</div>

### Persistence

<div align="justify">

The thermal state survives a reboot. `computeICP()` publishes `H` (with a timestamp) to a *retained* MQTT topic, and at boot the device reads it back, applies the elapsed cooling and resumes, so a breaker that was hot before a brief power blip is not treated as cold. Real overload episodes are also written to a **LittleFS file** (effectively unlimited) and published over MQTT; the `/icp_log` web page shows them in a sortable table, with the two record thresholds (danger % and peak A) editable in place. A **trip** is only recorded when the device actually lost power — a *cold* boot, not a soft `ESP.restart()`/OTA — with the model at its trip point **and** a trip-capable current, so ordinary reboots and firmware updates are never mislabelled as trips.

That recovery needs the network, and the case where it matters most is the one that does not have it: after a general power cut the router is booting too, so there is no MQTT to read the retained state from and no NTP to measure the elapsed time with. **Measuring, modelling and warning therefore do not wait for any of it.** The PZEM read, the thermal model, the forensic log, the alert latches, the buzzer and the LCD all run from the first loop of `setup()`, and every blocking wait in the boot sequence keeps them running — WiFi (up to 2x20 s), NTP (30 s), MQTT (~5 s) and the retained-state wait (3 s) add up to a worst case of about 75 s, which is exactly the minute you switch the whole house back on at once. Only the rule engine (which fires webhooks) and the MQTT publish wait for the network.

Recovering the retained value and measuring are complementary rather than alternatives: the retained value knows about heat accumulated **before** the reboot, when there was no current to infer it from, and the running model knows what has happened **since**. The device keeps the higher of the two, which errs towards warning early, and none of the bail-out paths discards the measured state any more.

</div>

> ℹ️ **Calibration.** The factory band is so wide that no theoretical curve is exactly right for one physical breaker. The defaults (`k = 1.07`, `tau = 384 s`, worst-case sensitivity) follow the UNE 20317 image above and deliberately err on the early side. The episode log exists so the model can later be refined against **real trips** rather than nameplate figures.

<div align="center">
  <img src="docs/icp_history.png" alt="ICP overload history page: sortable episode table with editable record thresholds" width="620">
  <br>
  <em>The <code>/icp_log</code> forensic page: every overload episode with its duration, peak current and peak danger level, sortable by any column. The two record thresholds (danger % and peak A) are edited in place; trips are always kept.</em>
</div>

---

## ⚙️ Rules / Triggers

<div align="justify">

Beyond the built-in alerts, the web panel carries a small **rules engine** so the device can act on its own instead of only warning you. A rule fires one or more actions when your conditions on the live measurements are met, which is enough to automate things like shedding a load before the ICP trips.

</div>

<div align="center">
  <img src="docs/ruleA.png" alt="Rule that turns the water heater off under load" width="380">
  <img src="docs/ruleB.png" alt="Rule that turns the water heater back on" width="380">
  <br>
  <em>Two rules acting as a simple load manager (the panel is shown in Spanish): switch a water heater off while the ICP is under stress, and back on once there is headroom.</em>
</div>

### How a rule works

<div align="justify">

Each rule has a **WHEN** part (the conditions) and a **THEN** part (the actions).

**WHEN.** Up to **3** conditions, each one a measurement, an operator and a value:

- Measurements: **Current** (A), **Voltage** (V), **Power** (W), **Power factor**, **Frequency** (Hz), **ICP load** (%) and **Energy** (kWh).
- Operators: `>`, `>=`, `<`, `<=` and `=` (the `=` operator allows a small tolerance, so a slightly noisy reading still matches a round value).
- With two or more conditions you pick **AND** (all of them must hold) or **OR** (any one is enough).
- If the meter returns an invalid reading, that condition counts as unknown and the rule neither fires nor clears on it.

**THEN.** Up to **2** actions per rule, and you can mix the two kinds:

- **Publish MQTT**: a topic, a message sent when the rule activates, an optional message sent when it clears, and a *retained* flag.
- **Webhook**: a URL called over HTTP as **GET** or **POST**, with an optional body for the activate and clear edges. On GET the body travels as a `?msg=` query parameter. On POST it is the request body with `Content-Type: application/json`. HTTPS works but the certificate is not validated, each call times out after 3 s, and at most one webhook runs per measurement cycle.

Actions are **edge-triggered**: the activate message is sent once when the conditions start holding, and the clear message once when they stop. Leave the clear message empty to do nothing when the rule releases. The messages are fixed text, with no templating, so live values are not inserted into the topic, URL or body.

</div>

### Debounce, testing and limits

<div align="justify">

- **Persistence (readings)**: from 1 to 20, and 3 by default. The condition must hold for that many consecutive readings before the edge fires, which debounces a flickering measurement. The real time this takes is the number of readings times the refresh interval.
- **Test now**: fires the rule's actions straight away, so you can check the topic or URL without waiting for the condition to happen.
- Up to **16** rules (each with up to **8** conditions and **4** actions) are stored in the **LittleFS filesystem**, so they survive reboots and updates. The on/off latch itself lives only in RAM, so after a reboot a condition that is still true sends its activate message once more. That is harmless for a retained MQTT topic, but worth keeping in mind for a webhook that is not idempotent.

</div>

### Worked example: load-shedding the water heater

<div align="justify">

The two rules in the screenshots drive a water heater through its own MQTT topic (`cmnd/calentador/Power`) so it does not add load while the ICP is close to tripping. It is the same heater the Rainmeter skin reports as `calentador_estado` and `calentador_corriente`.

- **Turn the heater off** (*Apagar termo*): when **ICP load > 0%** OR **Current > 30 A**, publish `OFF`, retained.
- **Turn the heater on** (*Arrancar termo*): when **ICP load = 0%** AND **Current < 18 A**, publish `ON`, retained.

Together they keep the heater running only while there is comfortable headroom, and cut it the moment the breaker starts to heat up or the current climbs.

</div>

> ℹ️ The editor loads and saves the whole rule table over `GET /json_rules` and `POST /save_rules`, and `POST /rule_test` backs the *Test now* button. Writes require a JSON content type as a basic CSRF guard.

---

## 📊 Consumption history

<div align="justify">

The device keeps the energy it measures at three granularities, all in the flash filesystem so none of them is capped by the tiny EEPROM sector:

- **Monthly** — one total per calendar month, kept **for the life of the device** (no 24-month ceiling). The energy counter is reset on each month change.
- **Daily** — one total per day.
- **Hourly** — one total per hour, stored *inside* each day.

The `/consumos` page renders it all as bar charts: three KPI tiles (today, this month, daily average), a monthly chart and a daily chart, with the value shown in white on each bar and the current period highlighted in blue. To see a day hour by hour, **tap its bar in the daily chart** (or pick any date in the *Hourly usage* card) and the 24 hours of that day are drawn below.

</div>

<div align="center">
  <img src="docs/cons_history.png" alt="Electricity usage page: KPI tiles, monthly and daily bar charts, and a per-day hourly drill-down" width="620">
  <br>
  <em>The <code>/consumos</code> page: KPIs, monthly (unlimited) and daily charts, and an hourly breakdown you reach by tapping a day or picking a date.</em>
</div>

> ℹ️ Served over `GET /consumo` (monthly + daily + the live current period) and `GET /json_hours?y=&m=&d=` (one day's hours). Day/month boundaries are checked once a minute; the monthly file uses an upsert (one record per month), the daily and hourly files are append-only.

---

## 🖥️ Rainmeter skin

<div align="center">
  <img src="docs/rainmeter.png" alt="MULTIMETREITOR Rainmeter skin on the Windows desktop" width="400">
  <br>
  <em>Rainmeter skin on the Windows desktop</em>
</div>

<div align="justify">

`Rainmeter/Multimetreitor/` contains a skin that displays the MULTIMETREITOR metrics directly on the Windows desktop, consuming the data over MQTT.

**Skin features:**

- Reads from the MQTT broker through Rainmeter's **[MqttClient](https://github.com/anschnapp/MqttPlugin)** plugin and parses the JSON published by the firmware.
- Shows: **Voltage, Frequency, Current, ICP (progress bar), Power, Power Factor and monthly Consumption**.
- **Visual warnings**: a red background on *Current* when it exceeds 30 A, and an ICP bar proportional to the thermal load (width is `ICP * 2.5`).
- Automatically fixes locale decimals (comma to dot) and the connection status for internal calculations (`Substitute`).
- Includes optional support for a **water heater** (`calentador_estado`, `calentador_corriente`) that lights up red when it is off. This is the same load driven by the [Rules / Triggers](#-rules--triggers) example.
- **Bilingual labels (ES and EN)**: click the language button (top-right of the skin) to switch. The choice is saved in the `Language` variable (see [Languages](#-languages-es--en)).

</div>

### Installing the skin

<div align="justify">

1. Install [Rainmeter](https://www.rainmeter.net/).
2. Install the **MqttClient** plugin (copy the `.dll` into `Rainmeter/Plugins`).
3. Copy the `Rainmeter/Multimetreitor` folder into `Documents/Rainmeter/Skins/`.
4. Edit the `[Variables]` section of the `.ini` and set:
   - `MQTT_BROKER`: your MQTT broker IP.
   - `MQTT_TOPIC`: the firmware's state topic (`electricidad/casa/estado`).
5. Load the skin from Rainmeter (*Refresh all* or *Manage*).

</div>

> ℹ️ By default the `.ini` ships with `MQTT_TOPIC=rainmeter/multimetreitor`. Change it to the topic the firmware actually publishes (`electricidad/casa/estado`), or adapt the topic on the device.

---

## 🌍 Languages (ES / EN)

<div align="justify">

Both UIs are bilingual and **default to Spanish**. Only the labels and UI text are translated. The metric values and units (V, A, W, and so on) are language-neutral.

</div>

### Web panel

<div align="justify">

- Click the **`EN` / `ES` button** at the top-right of the page to switch language instantly (client-side, no reload).
- The choice is remembered per browser via `localStorage` (`mmt_lang`).
- To add or tweak strings, edit the `I18N = { es: {...}, en: {...} }` dictionary in the embedded `<script>` of `multimetreitor.ino`. Translatable elements are marked with `data-i18n="key"`.

</div>

### Rainmeter skin

<div align="justify">

- Click the **language button** at the top-right of the skin to toggle between ES and EN. The choice is persisted to the `Language` variable in the `.ini` (`!WriteKeyValue` and `!Refresh`).
- This uses Rainmeter's standard localization pattern: a `Language` variable in `[Variables]` plus `@Include=#@#Lang_#Language#.inc`.
- Translations live in `Rainmeter/Multimetreitor/@Resources/Lang_ES.inc` and `Lang_EN.inc`. To change the default, set `Language=ES` (or `EN`) in `[Variables]`.

</div>

---

## 🛠️ Build and flash

<div align="justify">

**Requirements (Arduino IDE or arduino-cli):**

- The **ESP8266** Arduino core.
- Libraries: `PubSubClient`, `LiquidCrystal_I2C`, `PZEM004Tv30`, `ArduinoJson`, `ESP8266WebServer`, `ArduinoOTA`, `EspSoftwareSerial`.

**Steps:**

1. **Create your `secrets.h`** (see below) with your WiFi credentials.
2. Open `multimetreitor.ino` in the Arduino IDE.
3. Select the matching ESP8266 board.
4. Adjust the static IP and network configuration in `multimetreitor.ino` if needed.
5. Upload over USB the first time. After that you can update over **OTA** (hostname `multimetreitor-ota`).

</div>

### 🔐 Credentials (`secrets.h`)

<div align="justify">

WiFi credentials are kept **out of the source tree** in a `secrets.h` file that is git-ignored, so they are never committed. The repo ships a template, `secrets.h.example`.

To configure after cloning:

</div>

```bash
cp secrets.h.example secrets.h
```

<div align="justify">

Then edit `secrets.h` with your own values:

</div>

```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
```

<div align="justify">

`multimetreitor.ino` includes it via `#include "secrets.h"` and uses `WIFI_SSID` and `WIFI_PASSWORD`. The WiFi password is also reused as the OTA password.

</div>

> ℹ️ The static IP (`192.168.1.24`), gateway and hostnames are configured directly in `multimetreitor.ino`. Adjust them to your network.

---

## 🏠 Network note

<div align="justify">

MULTIMETREITOR is designed as a **local home-network appliance**. The web panel keeps things fast and simple and is meant to live inside your own trusted Wi-Fi. Because of that, **run it on a secure, trusted network and do not expose it directly to the Internet** (no port-forwarding). That is the intended setup. For multi-user or untrusted environments you can add HTTP Basic Auth to the control endpoints.

</div>

---

## 📝 License

<div align="justify">

Licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE).

Copyright (c) tonikelope

</div>
