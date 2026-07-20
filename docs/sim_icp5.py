# -*- coding: utf-8 -*-
"""Revision de la calibracion: tau=110 s salia de la MEDIA GEOMETRICA del rango
admisible, que es un criterio neutro. Para una alarma preventiva el sesgo debe
ir al lado RAPIDO (avisar antes), no al centro. Ademas hay que contrastar con
las medidas directas publicadas, no solo con las bandas."""
import math

# Medidas DIRECTAS publicadas (no bandas de lectura de curva):
#   Nergiza, ICP analogico real: a 2x la potencia contratada -> ~10 s al corte.
# Bandas de lectura de la curva publicada del ICP:
BANDAS = {1.20: (120, 600), 1.50: (27, 180), 2.00: (10, 90), 2.55: (1, 60)}
# Objetivo si priorizamos el lado rapido/observado:
OBJETIVO = {1.20: 120.0, 1.50: 27.0, 2.00: 10.0}


def t_iec(m, tau, k):
    den = m * m - k * k
    return None if den <= 0 else tau * math.log(m * m / den)


print("=== A. Contraste del ajuste tau=110, k=1.13 con la medida directa ===")
for m, obs in [(2.00, 10.0)]:
    t = t_iec(m, 110.0, 1.13)
    print(f"  {m:.2f}x -> modelo {t:.0f} s  vs  medida publicada {obs:.0f} s"
          f"   -> {t/obs:.1f}x MAS LENTO")
print("  (dentro de la banda 10-90 s, pero pegado al extremo lento)")

print("\n=== B. ¿Cabe un solo tau que reproduzca TODAS las observaciones? ===")
for m, obs in OBJETIVO.items():
    for k in (1.13,):
        f = math.log(m * m / (m * m - k * k))
        print(f"  {m:.2f}x = {obs:5.0f} s  exige  tau = {obs/f:6.1f} s   (k={k})")
print("  -> INCOMPATIBLES: la curva real es MAS INCLINADA que un primer orden")
print("     con asintota en 1.13. Hace falta subir k (asintota mas alta).")

print("\n=== C. Ajuste por minimos cuadrados en log(t) sobre el lado rapido ===")
mejor = None
for k10 in range(105, 126):
    k = k10 / 100.0
    # tau optimo analitico para minimizar suma de (log t_modelo - log t_obs)^2
    s = 0.0
    n = 0
    ok = True
    for m, obs in OBJETIVO.items():
        den = m * m - k * k
        if den <= 0:
            ok = False
            break
        s += math.log(obs) - math.log(math.log(m * m / den))
        n += 1
    if not ok:
        continue
    tau = math.exp(s / n)
    err = 0.0
    dentro = True
    for m, (lo, hi) in BANDAS.items():
        t = t_iec(m, tau, k)
        if t is None or not (lo <= t <= hi):
            dentro = False
    for m, obs in OBJETIVO.items():
        err += (math.log(t_iec(m, tau, k)) - math.log(obs)) ** 2
    rms = math.sqrt(err / n)
    marca = "OK bandas" if dentro else "  fuera   "
    print(f"  k={k:.2f}  tau={tau:6.1f} s   error rms x{math.exp(rms):.2f}   {marca}")
    if dentro and (mejor is None or rms < mejor[2]):
        mejor = (k, tau, rms)

k, tau, rms = mejor
print(f"\n  MEJOR: k = {k:.2f}, tau = {tau:.0f} s  (error medio x{math.exp(rms):.2f})")

print("\n=== D. Curva resultante para In = 25 A / 230 V ===")
In, V = 25.0, 230.0
print("  mult   corriente   potencia    frio     precargado 0.9xIn   banda/obs")
for m in [1.13, 1.18, 1.20, 1.30, 1.45, 1.60, 1.75, 2.00, 2.15, 2.55, 3.00]:
    t = t_iec(m, tau, k)
    den = m * m - k * k
    tc = tau * math.log((m * m - 0.81) / den) if den > 0 and m * m > 0.81 else None
    b = BANDAS.get(m)
    bs = f"[{b[0]:.0f}-{b[1]:.0f}] s" if b else ""
    if m in OBJETIVO:
        bs += f" obs {OBJETIVO[m]:.0f} s"
    ft = "no dispara" if t is None else f"{t:6.1f} s"
    fc = "  -" if tc is None else f"{tc:6.1f} s"
    print(f"  {m:.2f}   {m*In:6.2f} A   {m*In*V/1000:6.2f} kW  {ft:>11}   {fc:>12}      {bs}")

print("\n=== E. Comparacion de los tres candidatos a 64 A (2.55x) ===")
for nombre, kk, tt in [("centro geometrico (lo que propuse)", 1.13, 110.0),
                       ("extremo rapido con k=1.13", 1.13, 55.1),
                       (f"ajuste a observaciones k={k:.2f}", k, tau)]:
    print(f"  {nombre:38}: 2.55x -> {t_iec(2.55, tt, kk):5.1f} s"
          f" | 2.00x -> {t_iec(2.0, tt, kk):5.1f} s"
          f" | 1.20x -> {t_iec(1.2, tt, kk):6.1f} s")

print("\n=== F. Enfriamiento con la tau ajustada ===")
for t in [0, 30, 60, 120, 300]:
    print(f"  {t:>4} s -> {100*math.exp(-t/tau):5.1f} %")
print(f"  vuelta practica a frio (5 tau) = {5*tau/60:.1f} min")
