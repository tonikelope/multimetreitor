# Auditoría de rendimiento — MULTIMETREITOR — INFORME DEFINITIVO (jun 2026)

**Metodología:** 4 agentes de análisis en paralelo (sketch + fuentes reales de las librerías instaladas) → **revisión manual de cada afirmación crítica contra el código** → **medición empírica en el dispositivo real** (192.168.1.24) con firmware instrumentado por OTA (`micros()` por bloque + telemetría de heap), luego restaurado al firmware de producción.

---

## 1. Números MEDIDOS en tu hardware (no estimaciones)

Ciclo de 1000 ms, estado estable, 5 muestras + 1 con carga real de 2 kW:

| Bloque | Medido | Estimación previa de agentes |
|---|---|---|
| PZEM (6 getters → 1 transacción Modbus) | **59,3 – 63,5 ms** | 35-60 ms ✔ (extremo alto) |
| LCD (clear + 2 setCursor + 32 chars) | **53,5 – 55,3 ms** | ~64 ms (≈, algo alto) |
| MQTT (2 publish retained) | **2,3 – 2,4 ms** | 2-4 ms ✔ |
| `configForm()` (al abrir la página web) | **206 ms** ⚠️ | "varios ms" ✘ (muy subestimado) |
| **Total ciclo ocupado** | **~118 ms / 1000 ms (12%)** | |

Heap en marcha: **~37 KB libres, bloque máximo contiguo ~36 KB, fragmentación 2-5%** (uptime corto; el riesgo es a semanas de uptime).

Desglose del LCD derivado de la medición: ~1,46 ms/carácter (6 transacciones I2C + 102 µs de delays por char), `clear()` ≈ 4 ms (2 ms de ellos `delayMicroseconds(2000)` = CPU muerta), `setCursor` ≈ 1,7 ms.

## 2. Veredicto sobre las afirmaciones de los agentes

| Afirmación | Veredicto |
|---|---|
| PZEM cachea 200 ms; 6 getters = 1 transacción de 10 registros | ✅ Confirmado leyendo `PZEM004Tv30.cpp:47,403-414` |
| Lectura PZEM fallida bloquea ~108 ms (`READ_TIMEOUT=100`) | ✅ Confirmado (`:50,:526`); con `yield()` en el bucle (`:534`) |
| LCD: 6 transacciones I2C por carácter; 2 ms muertos en `clear()` | ✅ Confirmado (`LiquidCrystal_I2C.cpp:134,255-279`) y medido |
| "Saltar el redibujado cuando no cambia nada" ahorra ~54 ms/ciclo | ⚠️ **Matizado:** el voltaje jitterea cada segundo (225,1→225,6 V), así que la línea cambia casi todos los ciclos. El ahorro real viene de diff **por línea** (~25-29 ms típico) o **por carácter** (~45 ms típico) |
| `mqtt.connect()` bloquea 15 s si el broker cae | ⚠️ **Corregido:** el caso común (host caído) es el timeout TCP de **5 s** (`WiFiClient.cpp:80`); los 15 s (`MQTT_SOCKET_TIMEOUT`, `PubSubClient.h:36`) solo si el TCP conecta pero el broker calla. Ambos configurables: `setSocketTimeout()` existe (`:140`) |
| Boot bloqueante ~50 s peor caso | ⚠️ **Corregido al alza: ~80 s** (NTP 30 s `:439` + MQTT 5×(5+1) s ≈ 30 s `:964` + ICP 20 s `:438`). `setupWeb()` sí corre antes de `recoverICP()` (`:1943-1944`, el agente erró el orden) pero da igual: nadie llama a `handleClient()` durante las esperas |
| `configForm()`: String de ~22 KB + 32 replace, detonante nº 1 de OOM | ✅ Confirmado y **peor de lo estimado**: `MAIN_html` = 22.109 bytes medidos; **206 ms** de loop congelado por carga de página |
| Hot path por segundo sin heap; EEPROM solo por evento; retained gratis en device; `mqtt.loop()`/`handleClient()` idle ≈ µs | ✅ Todo confirmado |
| 888 B en 4 `static char out[]` innecesarios (el WebServer streamea síncrono sin copiar) | ✅ Confirmado (`ESP8266WebServer-impl.h:505-515`) |
| Hallazgo nuevo (mío) | En ciclo con lectura fallida, solo `voltage()` devuelve NAN; los otros 5 getters devuelven **valores rancios en silencio** (la caché se marca antes de leer, `:405`). No es de rendimiento, pero conviene saberlo |

## 3. Mejoras definitivas, por prioridad ganancia/riesgo

### P1 — LCD: eliminar `clear()` + redibujar solo líneas cambiadas
**Gana ~25-29 ms/ciclo típico** (de 54 a ~27 cuando cambia 1 línea; a ~0 si nada cambia). Variante por-carácter: gana ~45 ms/ciclo (solo se reescriben los 2-4 dígitos que cambian). Elimina el parpadeo. **Riesgo 🟢 bajo** (padding a 16 chars obligatorio). Esfuerzo: ~20 líneas.

### P2 — `configForm()`: streaming por trozos (patrón `/consumo` ya existente en el sketch)
**Gana: 206 ms de congelación → ~10-20 ms** y pico de heap de ~22-43 KB → <1 KB, eliminando el único detonante real de OOM por fragmentación a largo uptime. **Riesgo 🟡 medio** (trocear 22 KB en ~33 segmentos PROGMEM sin romper el HTML). Paliativo de 1 línea mientras tanto: `html.reserve(22500)` (🟢). Solo afecta al abrir la página de config.

### P3 — Timeouts de red: boot y broker caído
- `mqttClient.setSocketTimeout(3)` + `espClient.setTimeout(3000)`: peor stall recurrente 5-15 s → 3 s. 🟡 (validar en tu LAN)
- `ICP_RECOVER_TIMEOUT_MS` 20000 → 3000: el retained llega en <1 s si existe; el timeout entero solo se agota cuando no hay nada que recuperar. 🟢
- Pumpear `server.handleClient()` dentro de las esperas de boot (o NTP no bloqueante reutilizando `keepSyncNTP()`, que ya implementa la máquina de estados): web disponible en segundos, no en ~80 s peor caso. 🟢→🟡 (acoplamiento NTP↔recuperación ICP)

### P4 — Throttle del publish retained de ICP recovery
Republica idéntico cada segundo; con publicarlo cada 30-60 s o al cambiar `round(icpCarga)` basta (el cooldown se recalcula por tiempo transcurrido, `:1056-1060`). **Gana ~1,2 ms/ciclo + mitad de tráfico MQTT + sin churn del store retained del broker.** 🟢

### P5 — Microajustes (1 línea cada uno)
- `espClient.setNoDelay(true)`: hasta ~40 ms menos de latencia de entrega MQTT (Nagle). 🟢
- 4 `static char out[]` → stack: +888 B de heap permanente. 🟢

### P6 — Descartados / solo bajo síntoma
- **PZEM**: ya está en su suelo físico (~60 ms: 33 bytes a 9600 baudios + turnaround del sensor, no negociable). No escalonar lecturas (rompería la caché = regresión). UART hardware (`Serial.swap()`) solo si monitorizas NAN frecuentes: 🔴 recableo, GPIO15 en boot, pierdes consola USB.
- **`Wire.setClock(400k)`**: tras P1 es irrelevante; los backpack PCF8574 con pull-ups de 4,7 kΩ son marginales a 400 kHz. Solo si P1 no bastara, y probando 200 kHz en banco.
- **EEPROM, `logMessage`, `mqtt.loop()`, OTA/MDNS idle**: verificados como ya-óptimos o coste ~µs. No tocar.

## 4. Resultado esperado del paquete recomendado (P1+P2+P3+P4+P5)

| Métrica | Hoy (medido) | Tras el paquete |
|---|---|---|
| Ciclo ocupado | ~118 ms (12%) | **~63-90 ms (6-9%)** — el suelo es el PZEM |
| Congelación al abrir la web de config | ~206 ms + pico 22-43 KB heap | ~10-20 ms, pico <1 KB |
| Peor stall con broker caído | 5-15 s recurrente | ~3 s |
| Peor caso de boot sin web | ~80 s | ~5-10 s |
| Parpadeo de LCD | cada segundo | eliminado |
| Heap libre permanente | ~37 KB | ~38 KB y sin el alloc gigante |

Riesgo agregado del paquete: bajo (solo P2-completo y el timeout MQTT son 🟡, ambos testeables por OTA con rollback trivial).

---

## 5. Resultados POST-implementación (medidos en el dispositivo, jun 2026)

El paquete completo se implementó y verificó con la misma instrumentación `micros()` del baseline. Dos iteraciones durante la auto-auditoría:

- El diff **por línea** del LCD resultó insuficiente (este LCD muestra valores que jitterean en ambas líneas cada segundo → 60 ms, peor que el baseline por el padding). Se sustituyó por diff **por carácter** con coalescencia de huecos ≤2.
- El streaming del form con chunks de 1 KB añadía round-trips TCP (326 ms); se subió a chunks de 2.920 B (2 segmentos TCP) en heap transitorio.

| Métrica | Antes (medido) | Después (medido) |
|---|---|---|
| LCD por ciclo | 53,5-55,3 ms (redibujado completo + clear) | **7-9 ms típico, ~50 µs sin cambios, ~16 ms pico** |
| Ciclo total ocupado | ~118 ms | **~70 ms** (PZEM ~61 ms = suelo físico) |
| MQTT por ciclo | 2,3-2,4 ms | 2,0-2,1 ms |
| Página config: heap | pico ~22-43 KB (String 22 KB + replaces) | **sin pico** (acumulador transitorio de 2,9 KB) |
| Página config: total cliente | 321-465 ms (HEAD re-medido) | **224-383 ms** |
| HTML generado | — | **byte-idéntico al baseline** (gate de regresión) |
| Topic `icp` retained | ~35 msg/35 s | **2 msg/35 s** (cambio de valor o refresco 60 s) |
| Topic `estado` | 1/s | 1/s (sin cambios) ✓ |
| RAM estática | 34.728 B (43%) | **33.488 B (41%)** |
| Heap libre en marcha | ~36,9-37,4 KB | ~37,5-38,5 KB |
| Boot → web responsiva | tras todas las esperas (hasta ~80 s peor caso) | **~2-4 s tras WiFi** (handleClient bombeado en esperas NTP/MQTT/ICP) |
| Timeout broker caído | 5-15 s/intento | 3 s/intento (`setSocketTimeout(3)` + `setTimeout(3000)`) |
| Parpadeo LCD | cada segundo (clear) | eliminado |

Verificación sin regresiones: HTML de configuración byte-idéntico, los 7 endpoints HTTP con respuesta correcta, `estado`/`alertas_config` MQTT intactos, recuperación ICP por retained funcionando tras múltiples reinicios OTA.
