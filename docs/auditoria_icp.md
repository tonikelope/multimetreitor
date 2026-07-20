# Auditoría del modelo ICP — MULTIMETREITOR (jul 2026)

**Alcance:** `computeICP()`, `recoverICP()`, la parte ICP de `evaluateAlerts()` y la cadena de medida que los alimenta.

**Metodología:** lectura del código (`multimetreitor.ino`) + réplica del algoritmo en Python para simular escenarios y cuantificar desviaciones + investigación documental (normativa del ICP, comportamiento real del PZEM-004T v3 y, finalmente, **la curva oficial del aparato concreto**). Todas las cifras son calculadas o citadas. Lo que no he podido verificar está marcado como tal.

**Aparato modelado:** **Merlin Gerin C32N ICP-M de 25 A** (≈5,75 kW a 230 V), gama Multi 9. Poder de corte 4500 A, calibres 1,5-40 A, norma UNESA RU 6101-C y UNE 20317-88, homologado en el **BOE nº 214 de 7-9-1987**. Referencias del 25 A: 12288 (1P), **12303 (1P+N)**, **12318 (2P)**.

> **Nota sobre la antigüedad:** el C32N ICP-M se homologó en **septiembre de 1987** y seguía en lista de precios en **diciembre de 2007**. Tu aparato es por tanto de **1987-2008**, es decir **como mucho ~39 años**, no 40-50. La estimación inicial se quedaba justo fuera por poco. (El C32N *genérico* de curvas U/D, otro producto, sí se retiró hacia 1993.)

> **Estado:** las secciones 1-6 describen el firmware **anterior** al cambio y el análisis que lo motivó. Lo que se implementó finalmente, con las correcciones de la auditoría adversaria, está en **§9**.

---

## 1. Veredicto

El modelo es **estructuralmente correcto** —integra `dt/t_disparo(I)` sobre una curva interpolada logarítmicamente, el planteamiento clásico de un relé de tiempo inverso— y está bien protegido en los bordes. La ingeniería está cuidada.

Le falta lo que define físicamente a un bimetálico: **la memoria térmica**. El modelo actual es un *acumulador de tiempo de sobrecarga*, no una imagen térmica. De ahí los hallazgos serios H2 y H3.

Y hay un dato que reordena todas las prioridades: **la propia curva oficial del fabricante es una banda que abarca un factor ~80 en tiempo** (§3). Ninguna curva determinista puede ser "la correcta" para tu aparato; sólo la calibración contra sus disparos reales (H11) reduce esa incertidumbre.

| # | Hallazgo | Sentido del error | Severidad |
|---|---|---|---|
| **H1** | Disparo instantáneo a 2,15×In: el umbral magnético real está en **5,4-8,8×In (135-220 A)** | Falsas alarmas | **Alta** |
| **H2** | Sin memoria térmica: la carga previa se ignora (curva "desde frío" siempre) | **Avisa tarde o no avisa** | **Alta** |
| **H3** | Enfriamiento lineal + zona muerta entre 1,00× y 1,13×In | **Avisa tarde** | **Alta** |
| **H4** | El recorte de `dt` descarta tiempo real de sobrecarga | **Avisa tarde** | Media |
| **H5** | Tras una lectura Modbus fallida, `current` es un valor rancio y no se detecta | Ambos | Media |
| **H6** | `recoverICP()` no acota por arriba el valor recibido del broker | Falsas alarmas | Media |
| **H7** | La alerta ICP no tiene anti-*flapping* y exige `mult ≥ 1,13` para mostrarse | Ambos | Media |
| **H8** | Por encima de ~1,9×In el modelo es todo-o-nada (resolución del muestreo) | Falsas alarmas | Baja |
| **H9** | El PZEM entrega ⟨I⟩ promediado; el bimetal responde a ⟨I²⟩ | Avisa tarde | Baja |
| **H10** | Curva sin validar monotonicidad; calibres no normalizados en la UI | — | Baja |
| **H11** | No hay registro de eventos ni calibración con disparos reales | — | **Clave** |
| **H12** | Coste computacional | despreciable — **no tocar** | — |

---

## 2. Cómo funciona hoy

`computeICP()` (`multimetreitor.ino:1605`), una vez por ciclo de `refreshInterval`, desde `readSensorsAndTriggerAlerts()` (`:2220`):

1. `dt` real por `millis()`, recortado a `2 × refreshInterval` (`:1618`).
2. `mult = current / icpNominal`.
3. Si `mult ≥ 2,15` → `icpCarga = 100 %` y `return` (`:1625`).
4. Si `mult < 1,13` → resta `100·dt / icpCooldownTime` (lineal, 600 s por defecto).
5. Si no → interpola log-log entre los 6 puntos configurables y suma `100·dt / t_salto`.
6. Satura a [0, 100].

La alerta salta si `icpEnabled && icpCarga ≥ icpUmbral && mult ≥ 1,13` (`:1864`).

---

## 3. La curva oficial del C32N ICP-M

Localizada en el catálogo *Aparamenta carril DIN y cofrets modulares* de Merlin Gerin (2003, pág. 81) y en el *Catálogo abreviado* de 2004, cuya oferta ICP-M son exactamente el C32N ICP-M y el C60N ICP-M. Schneider publica **una sola curva ICP-M** para toda la gama Multi 9 (calibres 1,5-63 A); no existe una curva específica del C32N distinta de ésta.

La gráfica es vectorial; se digitalizó extrayendo las trayectorias del PDF, con verificación visual directa del gráfico. Tiene **dos trazos**:

| I/In | Trazo continuo (rápido) | Trazo discontinuo (lento) |
|---|---|---|
| asíntota | **1,13** | **1,45** |
| 1,45× | 5,1 s | 930 s |
| 2,00× | 1,71 s | 156 s |
| 2,55× | 0,90 s | 71 s |
| 3,00× | 0,62 s | 47 s |
| 5,00× | 0,24 s | 18 s |
| magnético | **5,39×In** | **8,80×In** |

Texto literal del catálogo: *"Según normas UNESA: RU 6101 C / UNE 20.317-88. Los relés magnéticos de los ICP-M actúan entre 5 In y 8 In."*

Las dos asíntotas, **1,13 y 1,45**, son exactamente las corrientes convencionales de no funcionamiento (Inf) y de funcionamiento (If) de la norma. La envolvente cubre la tolerancia de fabricación **y** el estado térmico de partida.

**Dos consecuencias que mandan sobre todo lo demás:**

**(a) La banda es enorme.** A 2×In el catálogo admite desde **1,71 s hasta 156 s**: un factor **91**. A 2,55×, factor 79. A 5×, factor 75. Tu aparato concreto está en un punto desconocido de esa banda, y ninguna fuente puede decir dónde. Esto es lo que convierte H11 en la mejora clave y no en un extra.

**(b) El modelo térmico de primer orden reproduce los dos trazos con un solo par de parámetros.** Al despejar qué precarga explicaría el trazo rápido partiendo del ajuste al trazo lento, sale un valor **constante dentro del 0,3 %** en todos los puntos (2×, 2,55×, 3×, 4×, 5×) — y ese valor es justo la asíntota. Es decir: el trazo rápido es el mismo aparato **precalentado al límite**, no una curva ajena. Que la ecuación de IEC 60255-149 genere sola las dos envolventes del catálogo es la mejor validación posible del modelo propuesto en §4.

---

## 4. Hallazgos

### H1 — El disparo instantáneo a 2,15×In (rectificado, y confirmado por catálogo)

**Corrección de una versión anterior de este informe:** mi primera evaluación de la curva por defecto `{2700, 900, 180, 25, 7, 1}` usaba bandas de fuentes secundarias españolas (artículos divulgativos que citan "la curva UNE 20317"). **Esas bandas no coinciden con el catálogo del fabricante**, y las conclusiones que saqué de ellas ("2,9× demasiado lento a 1,20×", "10× demasiado rápido a 2,00×") **no se sostienen**. Contra la curva oficial:

| I/In | Corriente | Firmware | Banda oficial (rápido - lento) | Veredicto |
|---|---|---|---|---|
| 1,20× | 30,0 A | 1718 s | 10,3 s - no dispara | dentro |
| 1,45× | 36,2 A | 180 s | 4,1 s - no dispara | dentro |
| 1,60× | 40,0 A | 25 s | 3,0 s - 329 s | **dentro** |
| 1,75× | 43,8 A | 7 s | 2,3 s - 222 s | **dentro** |
| 2,00× | 50,0 A | 1 s | 1,65 s - 142 s | 1,6× por debajo del límite rápido |
| 2,15× | 53,8 A | **instantáneo** | 1,38 s - 116 s | **fuera** |
| 2,55× | 63,7 A | **instantáneo** | 0,93 s - 74,6 s | **fuera** |

La curva que trazaste está **dentro de la envolvente oficial en casi todo su recorrido**. Quien la dibujó no iba desencaminado.

**Lo que sí queda confirmado, y ahora con dato de fabricante:** el disparo instantáneo a partir de 2,15×In no existe. El catálogo dice explícitamente que los relés magnéticos del ICP-M actúan **entre 5 y 8 veces In** — para tu 25 A, entre **135 A y 220 A**. El firmware declara instantáneo a **53,75 A**, entre 2,5 y 4 veces por debajo del umbral magnético real. Entre 2,15× y 5,4× sólo actúa el bimetal, y el catálogo le da **de 1,4 a 116 segundos**.

Impacto práctico: una sobrecarga sostenida de 54 A durante un segundo —dos hornos y la vitro— pone la barra en 100 % y mantiene la alarma los ~10 minutos que tarda en enfriarse, cuando el ICP real habría acumulado como mucho un 1 % de su capacidad térmica.

*(El falso positivo por corriente de arranque que este defecto haría temer no llega a ocurrir: el PZEM promedia sobre ~1,28 s y filtra los picos de 100-300 ms. Ver H9.)*

### H2 — Sin memoria térmica: se aplica siempre la curva "desde frío"

Las curvas de catálogo se entienden desde frío, y la norma reconoce el efecto del precalentamiento: el ensayo a 1,45×In se hace **desde caliente** (*"as from operating temperature, after I₁ > 1h"*). En el propio gráfico del C32N, el trazo rápido **es** el aparato precalentado (§3b).

El modelo actual no tiene estado térmico previo: si la corriente ha estado por debajo de 1,13×In, `icpCarga` se ha ido a 0 y la sobrecarga empieza a contar desde cero.

```
1 h a 0,95×In  ->  modelo: icpCarga = 0,0 %
                   imagen térmica real: 71 % del umbral de disparo
```

Con la calibración de §5, a 50 A (2×In): **135 s desde frío, 64 s si la casa venía de 25 A, 25 s si venía de 30 A**. Una casa que lleva horas cerca de la potencia contratada tiene el ICP a punto, y el modelo lo da por frío. Es exactamente el escenario en el que un aviso preventivo tiene valor.

### H3 — Enfriamiento lineal y zona muerta entre 1,00× y 1,13×In

**(a) El enfriamiento es lineal**, a ritmo fijo `100 %/cooldown` (`:1638`). El de un bimetal es exponencial de primer orden. Con el `cooldown` por defecto de 600 s:

| t | Modelo actual | Exponencial (τ = 600 s) |
|---|---|---|
| 300 s | 50,0 % | 60,7 % |
| 600 s | **0,0 %** | 36,8 % |
| 900 s | 0,0 % | 22,3 % |

El modelo declara el ICP completamente frío cuando aún le queda más de un tercio de calor.

**(b) Entre 1,00× y 1,13×In enfría a toda velocidad**, igual que con la casa apagada. Físicamente, a 1,10×In el bimetal se estabiliza en el **95 %** del punto de disparo:

```
300 s a 1,10×In partiendo del 90 %  ->  modelo: 40,0 %   |  real: ~95 %
```

Es el peor caso: la situación de riesgo más típica —consumo sostenido justo por debajo del límite— es la que el modelo interpreta como enfriamiento.

IEC 60255-149 avisa además del error clásico que aquí se comete: la constante de enfriamiento τ₂ sólo aplica con el equipo **sin corriente**; *"the heating thermal time constant τ1 is **also** used when the equipment is energized and the phase current is reduced to a lower level"*.

### H4 — El recorte de `dt` descarta tiempo real de sobrecarga

`:1618-1620` acota `dt` a `2 × refreshInterval` (2 s). Es razonable como defensa contra un ciclo retrasado, pero descarta tiempo durante el cual la sobrecarga seguía existiendo:

```
Sobrecarga a 1,60×In (t_disparo = 25 s en la curva actual), loop bloqueado:
  bloqueo  5 s -> modelo suma  8,0 %  | transcurrido real 24,0 %  | perdido 16 pp
  bloqueo 10 s -> modelo suma  8,0 %  | transcurrido real 44,0 %  | perdido 36 pp
  bloqueo 20 s -> modelo suma  8,0 %  | transcurrido real 84,0 %  | perdido 76 pp
```

Hay bloqueos reales: reconexión MQTT (hasta 3 s), OTA, resincronización NTP, carga de la página web. El recorte debería proteger contra saltos de reloj, no contra el funcionamiento normal: **un tope de 60 s cumple lo primero sin sacrificar lo segundo**.

### H5 — Tras un fallo de lectura del PZEM, `current` es un valor rancio y nadie lo detecta

En `PZEM004Tv30.cpp:403-414`, `updateValues()` marca `_lastRead = millis()` **antes** de leer. Si la transacción falla, `voltage()` devuelve NAN, pero las cinco llamadas siguientes del mismo ciclo entran por la rama de caché y **devuelven el valor anterior sin marcarlo como inválido**. Ya lo detectaste en la auditoría de rendimiento; lo que no estaba conectado es su efecto sobre el ICP:

- `if (isnan(current))` en `:1622` es, en la práctica, **código muerto**: nunca salta por un fallo de lectura.
- El modelo integra el valor del ciclo anterior como si fuera fresco. Si el fallo persiste —ruido en el `SoftwareSerial`, justo lo que pasa con cargas altas cerca del cableado— integra el mismo valor indefinidamente, congelado en alto o en bajo según el momento del fallo.

`voltage` es el único canario fiable.

### H6 — `recoverICP()` confía sin acotar en el valor del broker

`:1592-1594` sólo acota por abajo (`if (adjusted < 0) adjusted = 0`). **Falta el clamp superior a 100.** Un *retained* corrupto o manipulado con `{"valor": 5000}` deja `icpCarga = 5000` hasta el siguiente ciclo, y en ese hueco `evaluateAlerts()` ya puede disparar la alarma.

Dos detalles menores del mismo bloque:
- `getCurrentEpoch()` devuelve **0** si `!ntpOK` y **-1** si el epoch no es válido; sólo se comprueba `== -1` (`:1567`). Con 0, `secs = 0` y se recupera el valor íntegro sin descontar nada. Es conservador, pero es un camino no intencionado y sin log.
- La compensación asume que durante el apagón no hubo consumo. Correcto si el reboot vino de un corte real; erróneo si vino de un OTA o un watchdog con la casa cargada.

### H7 — La alerta ICP no tiene anti-*flapping*, y `mult ≥ 1,13` crea un falso negativo

El commit `2eb407f` dejó el ICP fuera de la persistencia e histéresis a propósito. Con `icpCarga` integrada la persistencia importa menos, pero la condición `mult >= 1.13f` (`:1864`) sí crea un agujero:

**Si `icpCarga` está al 99 % y el consumo baja a 1,10×In, la alarma desaparece** — justo cuando el ICP real está a un suspiro de saltar (H3b). Con el modelo corregido esa condición sobra: `icpCarga` ya contiene la información. Si se quiere evitar que la alarma quede colgada tras el pico, lo correcto es histéresis sobre `icpCarga` (limpiar por debajo de `umbral − 10 pp`).

### H8 — Por encima de ~1,9×In el modelo es todo-o-nada

Con `refreshInterval = 1000 ms` (~1,1 s reales por ciclo), lo que suma **una sola muestra** con la curva actual: 1,60× → 4,4 %; 1,75× → 15,7 %; 1,90× → 50,5 %; **2,00× → 110 %**. La curva tiene resolución útil sólo mientras `t_disparo >> refreshInterval`. Con la recalibración de §5 (que lleva la parte alta a decenas de segundos) el problema desaparece solo.

### H9 — El PZEM entrega ⟨I⟩; el bimetal responde a ⟨I²⟩

El PZEM-004T v3 expone los registros **promediados** del chip V9881D: ventana de ~**1,28 s**, estabilización **~3 s** (medido por terceros: detección de escalón 1,21-1,33 s, estabilización 2,53-2,65 s). El valor es RMS verdadero por ciclo, pero promediado linealmente.

El calentamiento va con I², y ⟨I²⟩ ≥ ⟨I⟩². Para pulsaciones más rápidas que ~1,3 s el modelo subestima: una carga al 50 % de ciclo entre 0 y 3×In se ve como 1,50× cuando su RMS real es 2,12×.

**Alcance real:** sólo afecta a pulsaciones sub-segundo. Vitrocerámica, horno y microondas ciclan con periodos de segundos o decenas de segundos y el modelo los sigue bien. El caso realista es una soldadora, una bomba con arranques repetidos o herramienta eléctrica. **Es una limitación del sensor, no del algoritmo: no se arregla leyendo más rápido** (no hay nada nuevo que leer dentro del chip). Lo correcto es documentarla.

*Efecto colateral:* el retardo de estabilización de ~2,5 s era un **250 % de la curva a 2,00×In** con la curva actual (1 s); con la calibración de §5 (135 s) baja al **1,9 %**. Otro argumento para recalibrar.

### H10 — Validación de la curva y calibres

- `handleConfigPost()` (`:2288-2296`) acota cada punto a [1, 7200] s **pero no comprueba que la curva sea decreciente**. Se puede guardar `{1, 5000, 3, …}`, que interpola sin error pero describe un aparato imposible.
- `icpNominal` acepta cualquier valor entre 5 y 80 A con paso 0,1. Los calibres de ICP están fijados por el **RD 1725/1984**: 1,5 / 3 / 5 / 7,5 / 10 / 15 / 20 / 25 / 30 / 35 / 40 / 45 / 50 / 63 A. El C32N además sólo existió hasta 40 A. Un `<datalist>` con esa serie es gratis.
- `MIN_CURVE_TIME_S = 1` hace inútil el último tramo con muestreo de 1 s (H8).

### H11 — Sin registro de eventos ni calibración contra el aparato real — **la mejora clave**

Con una banda oficial de fábrica de factor ~80 (§3a), **ninguna curva teórica puede ser la de tu ICP**. Sólo hay una forma de averiguarlo: observarlo.

Un disparo real es observable — el ESP8266 está alimentado detrás del ICP, así que un corte se manifiesta como *reboot* con un *retained* de `icpCarga` alto y timestamp de hace pocos segundos. Registrar `{timestamp, icpCarga, corriente, duración}` de cada evento (16 entradas ≈ 192 B, que caben al final de `AppConfig` con el mismo patrón de *append* + magic que usaste para `rules[]`) permite cerrar el bucle: si saltó cuando el modelo iba por el 60 %, τ es demasiado grande; si el modelo se planta en 100 % repetidamente sin que el ICP se inmute, es demasiado pequeño.

Para la característica estrella, esto es lo que convierte una simulación genérica en un gemelo de *tu* aparato.

### H12 — Rendimiento: no tocar

`log10f` + `powf` por ciclo; el modelo propuesto usa `logf` + `expf`: **~15-25 µs por ciclo de 1000 ms**, frente a los 7-9 ms del LCD y los ~60 ms del PZEM ya medidos. Es el 0,002 % del ciclo. Precalcular es posible pero no está justificado.

---

## 5. Propuesta: modelo de imagen térmica calibrado al catálogo

### El modelo

`H` = nivel térmico normalizado, 0 = frío, 1,0 = disparo. La UI no cambia: `icpCarga = 100·H` significa lo mismo que ahora.

```
H_eq  = (I / (k·In))²                      equilibrio térmico
H(t)  = H_eq + (H(t-dt) - H_eq)·e^(-dt/τ)  IEC 60255-149, ec. 1
```

Curva desde frío (lo que se dibuja en la web): `t = τ·ln(m² / (m² − k²))`.
Curva con precarga Ip: `t = τ·ln((m² − Ip²) / (m² − k²))` — sale sola, no hay que programarla.

### Calibración contra la curva oficial

Ajustando el trazo lento del catálogo (§3) y tomando `k` en el centro de la banda normativa Inf-If: **`k = 1,30`, `τ = 246 s`**. Reproducción de ambos trazos:

| I/In | Catálogo frío | Modelo | Catálogo caliente | Modelo (precargado) |
|---|---|---|---|---|
| 2,00× | 156 s | 135 s | 1,71 s | 2,20 s |
| 2,55× | 71 s | 74 s | 0,90 s | 1,06 s |
| 3,00× | 47 s | 51 s | 0,62 s | 0,70 s |
| 5,00× | 18 s | 17 s | 0,24 s | 0,22 s |

Un solo par de parámetros reproduce las dos envolventes del fabricante dentro del error de digitalización.

**`k` debe quedar configurable, y es la decisión importante:** con `k = 1,30` el modelo no acumula nada por debajo de **32,5 A**. Si tu ejemplar concreto está en el extremo sensible de la banda (asíntota 1,13 → 28,3 A), el modelo no avisaría de una sobrecarga que a él sí le afecta. Tres ajustes razonables:

| Ajuste | k | τ | Asíntota (25 A) | Comportamiento |
|---|---|---|---|---|
| Conservador | 1,13 | ~295 s | 28,3 A | avisa antes; más falsas alarmas |
| **Típico (defecto)** | **1,30** | **246 s** | **32,5 A** | centro de la banda del catálogo |
| Permisivo | 1,45 | 191 s | 36,3 A | sólo avisa de sobrecargas claras |

### El código

```cpp
// Imagen térmica de primer orden (IEC 60255-149 ec. 1).
// icpCarga = nivel térmico en %, 100 % = punto de disparo.
void computeICP() {
  unsigned long now = millis();
  if (lastIcpMillis == 0) { lastIcpMillis = now; return; }
  float dt = (now - lastIcpMillis) / 1000.0f;
  lastIcpMillis = now;

  // Sólo protege contra saltos absurdos de reloj, NO contra el funcionamiento
  // normal: un bloqueo de 10 s a 1,6×In es tiempo de sobrecarga REAL (ver H4).
  if (dt <= 0.0f) return;
  if (dt > ICP_MAX_DT_S) dt = ICP_MAX_DT_S;   // 60 s

  // Lectura Modbus fallida: la caché de la librería marca _lastRead antes de
  // leer, así que 'current' queda rancio sin ser NaN. 'voltage' es el canario.
  if (isnan(voltage) || isnan(current)) return;   // congela, no integra basura

  float mult = (config.icpNominal > 0) ? (current / config.icpNominal) : 0.0f;
  float k    = config.icpK;                       // 1.30 por defecto
  float heq  = (mult * mult) / (k * k);           // equilibrio térmico

  // τ1 mientras haya corriente (aunque baje del umbral, IEC 60255-149 §7);
  // τ2 sólo con el equipo prácticamente sin carga.
  float tau = (mult > 0.05f) ? (float)config.icpTau : (float)config.icpTauCool;

  float h = icpCarga / 100.0f;
  h = heq + (h - heq) * expf(-dt / tau);

  icpCarga = 100.0f * h;
  if (icpCarga < 0.0f)   icpCarga = 0.0f;
  if (icpCarga > 100.0f) icpCarga = 100.0f;
}
```

Desaparecen: el caso especial de 2,15×, el `ICP_TRIP_FLOOR_S`, el bucle de interpolación y las dos ramas de calentamiento/enfriamiento. La tabla de 6 tramos se sustituye por `k` y `τ`; si prefieres conservarla como modo experto, `τ(m) = t_curva(m)/ln(m²/(m²−k²))` reproduce exactamente los tiempos configurados y aun así gana precalentamiento y enfriamiento correctos.

**Aviso de migración:** `icpCarga` deja de ser 0 en reposo. El equilibrio es `(I / 32,5 A)²`: 4 A → 1,5 %, 15 A → 21 %, 20 A → 38 %, 25 A → 59 %, 30 A → 85 %. Con `icpUmbral` al 75 %, un consumo sostenido de 28 A ya entraría en aviso — físicamente correcto, pero conviene revisar el umbral.

---

## 6. Tu instalación en unidades reales

ICP de **25 A** a 230 V = **5,75 kW** contratados. `k = 1,30`, `τ = 246 s`:

| Corriente | Potencia | Desde frío | Casa venía de 25 A | Casa venía de 30 A | Firmware actual |
|---|---|---|---|---|---|
| 30,0 A | 6,90 kW | no dispara | no dispara | no dispara | 28,6 min |
| 32,5 A | 7,47 kW | no dispara | no dispara | no dispara | 15,0 min |
| 36,2 A | 8,34 kW | 401 s | 242 s | 117 s | 180 s |
| 40,0 A | 9,20 kW | 266 s | 144 s | 62 s | 25 s |
| 43,8 A | 10,06 kW | 197 s | 100 s | 41 s | 7 s |
| 50,0 A | 11,50 kW | 135 s | 64 s | 25 s | 1 s |
| 53,8 A | 12,36 kW | 112 s | 52 s | 20 s | **instantáneo** |
| 63,7 A | 14,66 kW | 74 s | 33 s | 12 s | **instantáneo** |
| 75,0 A | 17,25 kW | 51 s | 22 s | 8 s | **instantáneo** |
| **135-220 A** | 31-51 kW | **disparo magnético** (<0,02 s) | | | — |

Las columnas de precalentamiento son lo que hoy no existe y lo que hace útil el aviso: **con la casa ya cargada el margen cae a la mitad o a un cuarto**.

---

## 7. Plan por prioridad (estado: P1, P2 y P4 implementados — ver §9)

**P1 — Corregir el modelo (H1, H2, H3, H8).** Sustituir `computeICP()` por la imagen térmica; eliminar el disparo instantáneo a 2,15×; calibrar con `k = 1,30`, `τ = 246 s`.

**P2 — Robustez (H4, H5, H6).** Tope de `dt` a 60 s; `isnan(voltage)` como canario; clamp superior en `recoverICP()` y log del caso `getCurrentEpoch() == 0`.

**P3 — Registro de eventos y calibración con disparos reales (H11).** Lo que de verdad ancla el modelo a tu aparato, dada la banda de factor 80.

**P4 — Alerta (H7).** Quitar `mult >= 1.13f`, histéresis sobre `icpCarga`.

**P5 — Cosmética útil (H10).** Monotonicidad de la curva, `<datalist>` de calibres, documentar H9 en el README.

**No hacer:** optimizar el coste computacional (H12).

---

## 8. Verificación propuesta

Siguiendo el método de la auditoría de rendimiento (baselines + instrumentación temporal + comparación byte a byte):

1. Endpoint temporal `/icp_test?mult=X&dt=Y` que ejecute el modelo en seco y devuelva la curva completa, para comparar contra las tablas de §5-6 **en el dispositivo real**.
2. Registro de `icpCarga` cada segundo durante un ciclo de lavadora/vitrocerámica real, con modelo viejo y nuevo.
3. Prueba de `recoverICP()` con un *retained* fabricado a mano (valor fuera de rango, timestamp futuro, `error`) antes y después del clamp.

---

## 9. Lo implementado (jul 2026)

Cambios aplicados en `multimetreitor.ino`, con auditoría adversaria posterior (tres revisiones independientes con lentes distintas: EEPROM, modelo/alertas, web) y sus correcciones ya incorporadas.

**Modelo (`computeICP`).** Imagen térmica de primer orden con `k` y `τ` configurables (defectos 1,30 y 246 s). Sin atajo de disparo instantáneo. Tope de `dt` a 60 s en lugar de 2×`refreshInterval`. Congelación —sin consumir el tiempo transcurrido— cuando `isnan(voltage)` delata una lectura Modbus fallida, para que el hueco lo integre el siguiente ciclo válido. Protección contra NaN en `k` y en `icpCarga` (un NaN sería absorbente: los clamps no lo atrapan). Centinela `icpPrimed` en vez de `lastIcpMillis == 0`, que colisiona con el paso de `millis()` por cero cada 49,7 días.

**Alerta (`evaluateAlerts`).** Dos vías independientes, ambas condicionadas a que la corriente supere 1,13×In (por debajo no dispara ningún ejemplar, así que un consumo legítimo estable nunca puede alarmar):

- **Inminencia:** avisa cuando el tiempo estimado hasta el disparo baja de 120 s, resolviendo el modelo hacia delante — `t = τ·ln((H_eq − H)/(H_eq − 1))`, definido sólo mientras `H_eq > 1`. Sin esto el aviso llegaba a los 94 s en una sobrecarga de 50 A, cuando un ejemplar de la rama rápida ya habría saltado. Se descartó la alternativa «corriente por encima de k durante 3 muestras» porque alarmaba con ráfagas de 35 A que aún tenían ocho minutos de margen.
- **Térmica:** `icpCarga ≥ icpUmbral`, que cubre la zona 1,13×-1,30×In donde un ejemplar sensible sí dispara y la vía de inminencia calla.

Histéresis de 10 puntos con suelo positivo (con `icpUmbral = 10` la condición de despeje era `icpCarga < 0`, inalcanzable, y la alarma quedaba enganchada para siempre).

**Persistencia.** `icpK`, `icpTau` e `icpModelMagic` añadidos al final de `AppConfig`, manteniendo todos los offsets previos (verificado con el compilador del target: `lcdLang`=340, `rules`=344, `sizeof`=3360 < 4096). El campo obsoleto de la curva se conserva **con los valores legacy, no a cero**: la versión anterior los valida al arrancar y habría borrado la configuración completa —incluido el histórico de energía— si alguien hiciera *rollback*. La migración se persiste con `saveConfig()` inmediato, para no reejecutarse en cada arranque pisando el `cooldown` guardado.

**Otros.** Banda muerta de 2 puntos en el topic MQTT retenido (con el modelo nuevo `icpCarga` sigue la carga de forma continua y el criterio anterior habría deshecho el *throttling* de `3d67fde`); `Cache-Control: no-store` en el formulario (una página cacheada de antes del OTA enviaría los campos viejos y el guardado parecería funcionar sin hacerlo); enfriamiento exponencial también en `recoverICP`, con clamp por arriba; `k` y `τ` publicados en `/json_alerts` y en el topic retenido de configuración.

**Verificación previa al despliegue.** Compila limpio (RAM 46 %, flash 49 %, IRAM 96 % — sin cambios respecto a HEAD). Tokens `%…%` del HTML y ramas del sustituidor en correspondencia exacta; claves i18n completas en ambos idiomas; JS validado sintáctica y funcionalmente en un DOM simulado. Simulación del comportamiento con la réplica exacta del código: sin alarma con 3/15/20/25/28 A sostenidos (tampoco con el umbral al 40 %), aviso a los 17 s con 50 A desde frío, a los 3 s con 64 A, a los 26 s con 40 A partiendo de una casa cargada, y silencio ante ráfagas de 40 A durante 2 s o arranques repetidos de 35 A durante 5 s.

**La barra sigue midiendo peligro, no temperatura.** El estado térmico interno no es cero en uso normal —una casa que consume sus 25 A contratados deja el bimetal al 59 %—, así que mostrarlo en crudo dejaría la barra permanentemente medio llena y desperdiciaría todo su rango útil. Lo que se publica en LCD, web, MQTT, Rainmeter y motor de reglas es `icpNivelPeligro()`: el trayecto recorrido entre «no puede saltar» y «salta ahora», tomando como origen el nivel térmico de equilibrio a 1,13×In.

```
nivel = (H − H_seguro) / (1 − H_seguro),   H_seguro = (1,13 / k)²
```

Con eso la barra marca **0 % en todo consumo que no pueda hacer saltar el ICP** (hasta 28,25 A con los valores por defecto) y llega a 100 % exactamente en el disparo. La memoria térmica no se pierde: si el bimetal ya venía caliente, cruza el punto seguro antes y la barra empieza a llenarse antes. Ejemplo, casa a 25 A que sube de golpe a 40 A: la barra arranca a los 48 s y llega a 100 % a los 143 s, que es justo cuando el modelo sitúa el disparo.

El estado térmico crudo se sigue usando internamente y es lo que se persiste en el topic *retained*, porque es lo que hay que restaurar tras un reinicio.

### Inicialización: no se puede medir la temperatura del bimetal

Es la limitación de fondo del modelo, y merece decirse explícitamente: **la temperatura real del ICP no es observable**. Sólo se puede estimar, y asumir «frío» al arrancar es la peor estimación posible — una casa que lleva horas con sus 25 A contratados tiene el bimetal al 59 %, y partir de 0 duplicaría el margen que el modelo cree tener en la siguiente sobrecarga (265 s en vez de 144 s a 40 A).

El arranque combina dos estimadores independientes y se queda con el mayor:

1. **Lo recuperado del topic *retained*** (`recoverICP`), con enfriamiento exponencial por el tiempo transcurrido. Es el único que conoce el calor acumulado *antes* del reinicio, y es el que manda cuando hubo corte de luz o disparo del ICP: al volver no hay corriente de la que deducir nada.
2. **El equilibrio térmico con la corriente medida en la primera lectura válida.** Lo que la casa esté consumiendo al arrancar es lo que probablemente llevaba consumiendo un rato. Se topa en el punto seguro (`H_seguro`), de modo que la semilla **nunca puede levantar una alarma por sí sola**: sitúa el calor de fondo y deja que todo lo que esté por encima se gane integrando tiempo real.

| Escenario de arranque | *Retained* | Semilla | Estado inicial | Barra |
|---|---|---|---|---|
| Corte de luz: 90 % hace 60 s, casa a 3 A al volver | 70,5 % | 0,9 % | **70,5 %** | 0 % |
| Reinicio OTA con la casa a 25 A | 53,9 % | 59,2 % | **59,2 %** | 0 % |
| Sin MQTT ni NTP, casa a 25 A | — | 59,2 % | **59,2 %** | 0 % |
| Sin MQTT, casa en reposo | — | 0,9 % | **0,9 %** | 0 % |
| Arranca durante un pico de 40 A | — | 151 % → topada | **75,6 %** | 0 % |

Y lo que quede de error se disuelve solo, porque el propio modelo es un filtro de primer orden: a los 10 minutos queda el 9 % del error de arranque, a los 20 minutos menos del 1 %.

`icpUmbral` pasa por tanto a significar «avísame cuando lleve el N % del camino al disparo», que es más intuitivo que el porcentaje de calor absoluto.

---

## 10. Lo que NO he podido verificar

- **El texto de UNE 20317** (1988 y 2005) es de pago. Las asíntotas 1,13/1,45 salen del gráfico del fabricante y coinciden con Inf/If de IEC 60898-1; el catálogo cita la norma pero no reproduce sus tablas.
- **La leyenda de los dos trazos.** El catálogo **no rotula** cuál es cuál en la gráfica ICP-M (las curvas B/C/D del mismo catálogo sí llevan leyenda: "límites de disparo térmico en frío" y "límites electromagnéticos"). La interpretación de §3 —tolerancia de fabricación combinada con estado térmico— es inferencia mía, apoyada en que las asíntotas coinciden con Inf/If y en la consistencia numérica del ajuste (§3b).
- **Los valores numéricos de la curva** son mi digitalización del gráfico vectorial, no una tabla publicada. Precisión ±10 % aprox., y poco fiable cerca de las asíntotas verticales. Schneider no publica tabla numérica.
- **No existe curva específica del C32N** distinta de la genérica ICP-M de Multi 9; Schneider publica una sola para toda la gama. Lo que cambia entre C32N y C60N es el poder de corte (4500 vs 6000 A) y el calibre máximo (40 vs 63 A).
- **Fuentes secundarias discrepantes.** Los artículos divulgativos españoles que citan "la curva UNE 20317" dan una banda distinta (1,2×: 2-10 min; 1,5×: 27 s-3 min; 2×: 10 s-1,5 min); sólo su extremo lento a 2× (~90 s) se aproxima al catálogo (156 s). **Para calibrar, el catálogo del fabricante manda.**
- **La deriva por envejecimiento** tras ~30 años no está cuantificada en ninguna fuente publicada. La industria admite que existe pero no la publica. No la modeles con un sesgo inventado: resuélvela con H11.
- **Temperatura ambiente** — sí tiene cifra normativa y es el efecto ambiental dominante: **−0,6 % de In por cada °C sobre 30 °C** (ABB, IEC 60898-1). Un ICP en una CGP a 45 °C tiene un umbral efectivo ~9 % más bajo (22,7 A efectivos en uno de 25 A); en enero a 5 °C, ~15 % más alto. El dispositivo no mide temperatura hoy.
- **El promediado interno del PZEM** no está documentado por Peacefair. La ventana de 1,28 s viene del datasheet del chip V9881D (identificado por ingeniería inversa) más medidas empíricas coincidentes. Nadie ha publicado un ensayo directo del PZEM contra un pico de arranque con instrumento de referencia.

---

## Fuentes

**El aparato**
- [Catálogo *Aparamenta carril DIN y cofrets modulares*, Merlin Gerin 2003 — curva ICP-M en pág. 81](https://www.construmatica.com/archivos/28205/distribucion_electrica_en_baja_tension/aparellaje_baja_tension_terminal_y_cofrets_modulares/catalogos/catalogo_aparamenta_carril_din_y_cofrets_modulares.pdf)
- [*Catálogo abreviado del instalador*, Merlin Gerin/Eunea/Telemecanique 2004 — C32N ICP-M y C60N ICP-M](https://www.construmatica.com/archivos/28205/distribucion_electrica_en_baja_tension/aparellaje_baja_tension_terminal_y_cofrets_modulares/catalogos/catalogo_abreviado_del_instalador.pdf)
- [Legrand — Inf/If bajo UNE 20317](https://assets.legrand.com/pim/NP-FT-GT/F01147FR-02.pdf)

**Normativa**
- [ABB, *Comparison of tripping characteristics for miniature circuit-breakers*, 2CDC400002D0201](https://library.e.abb.com/public/114371fcc8e0456096db42d614bead67/2CDC400002D0201_view.pdf) — tabla 7 de IEC 60898-1, coeficiente de temperatura, "tripping curve from cold state"
- [IEC 60255-149:2013 — extracto oficial](https://cdn.standards.iteh.ai/samples/19166/a873d7eb228d41db9ff7cbc6441738b1/IEC-60255-149-2013.pdf) — modelo de imagen térmica, ecs. 1-6
- [UNE 20317:2005](https://www.une.org/encuentra-tu-norma/busca-tu-norma/norma?c=N0032934) · [UNE 20317:1988](https://www.une.org/encuentra-tu-norma/busca-tu-norma/norma?c=N0001115)
- [Guía Técnica ITC-BT-17](https://industria.gob.es/Calidad-Industrial/seguridadindustrial/instalacionesindustriales/baja-tension/Documents/bt/ITC-BT-17_guia_E_Sep_20_R2.pdf) · [RD 1725/1984 (BOE)](https://www.boe.es/buscar/doc.php?id=BOE-A-1984-21985)
- [E-T-A — influencia de la temperatura ambiente](https://www.e-t-a.es/support/informacion_tecnica/influencia_de_la_temperatura/)

**Sensor**
- [Datasheet PZEM-004T v3.0](https://innovatorsguru.com/wp-content/uploads/2019/06/PZEM-004T-V3.0-Datasheet-User-Manual.pdf) · [Datasheet Vango V98XX/V9881D](https://blog.danman.eu/wp-content/uploads/2020/12/v98xx.pdf)
- [mandulaj/PZEM-004T-v30 issue #44](https://github.com/mandulaj/PZEM-004T-v30/issues/44) · [MycilaPZEM004Tv3 — benchmarks](https://github.com/mathieucarbou/MycilaPZEM004Tv3) · [TheHWcave — ingeniería inversa](https://github.com/TheHWcave/Peacefair-PZEM-004T-)

**Simulaciones de este informe:** `sim_icp.py` (réplica del algoritmo actual), `sim_icp2.py` (modelo propuesto), `sim_icp3.py` (promediado del sensor), `sim_icp4.py` (calibración inicial contra bandas secundarias, **superada**), `sim_icp5.py` (revisión del criterio), `sim_icp6.py` (calibración final contra el catálogo).
