# -*- coding: utf-8 -*-
"""Calibracion contra la CURVA ICP-M OFICIAL de Merlin Gerin (catalogo Multi 9
2003/2004, pag. 81), digitalizada del PDF vectorial y verificada visualmente.
Sustituye a las bandas de fuentes secundarias usadas antes."""
import math
from sim_icp import t_salto  # curva por defecto del firmware

# Curva oficial ICP-M: envolvente de dispersion (dos limites).
# Trazo continuo  = limite RAPIDO (asintota ~1.13, magnetico a 5.4 In)
# Trazo discontinuo = limite LENTO (asintota ~1.45, magnetico a 8.8 In)
RAPIDO = {1.20: 12.4, 1.45: 5.1, 1.50: 4.45, 2.00: 1.71, 2.55: 0.90,
          3.00: 0.62, 4.00: 0.35, 5.00: 0.24}
LENTO = {1.45: 930.0, 1.50: 679.0, 2.00: 156.0, 2.55: 71.0,
         3.00: 47.0, 4.00: 26.0, 5.00: 18.0}


def t_iec(m, tau, k):
    den = m * m - k * k
    return None if den <= 0 else tau * math.log(m * m / den)


def ajusta(obs, kmin, kmax):
    """(k, tau) por minimos cuadrados en log(t), ignorando puntos cercanos
    a la asintota (poco fiables al digitalizar)."""
    mejor = None
    pts = {m: t for m, t in obs.items() if m >= 2.0}
    for k100 in range(int(kmin * 100), int(kmax * 100) + 1):
        k = k100 / 100.0
        s, n = 0.0, 0
        ok = True
        for m, t in pts.items():
            den = m * m - k * k
            if den <= 0:
                ok = False
                break
            s += math.log(t) - math.log(math.log(m * m / den))
            n += 1
        if not ok:
            continue
        tau = math.exp(s / n)
        err = sum((math.log(t_iec(m, tau, k)) - math.log(t)) ** 2 for m, t in pts.items())
        rms = math.sqrt(err / n)
        if mejor is None or rms < mejor[2]:
            mejor = (k, tau, rms)
    return mejor


print("=== A. ¿Reproduce el modelo de 1er orden la curva OFICIAL? ===")
for nombre, obs, kr in [("LIMITE RAPIDO", RAPIDO, (1.00, 1.30)),
                        ("LIMITE LENTO ", LENTO, (1.20, 1.60))]:
    k, tau, rms = ajusta(obs, *kr)
    print(f"\n  {nombre}:  k = {k:.2f}   tau = {tau:.1f} s"
          f"   (error medio x{math.exp(rms):.2f})")
    print("    I/In   catalogo    modelo")
    for m, t in sorted(obs.items()):
        mt = t_iec(m, tau, k)
        flag = "" if m >= 2.0 else "  (asintota, no ajustado)"
        print(f"    {m:.2f}  {t:8.1f} s  {mt:8.1f} s{flag}")

kr_, tr_, _ = ajusta(RAPIDO, 1.00, 1.30)
kl_, tl_, _ = ajusta(LENTO, 1.20, 1.60)

print("\n=== B. Amplitud REAL de la banda del fabricante ===")
print("  I/In    rapido     lento     factor")
for m in [1.5, 2.0, 2.55, 3.0, 5.0]:
    a, b = t_iec(m, tr_, kr_), t_iec(m, tl_, kl_)
    print(f"  {m:.2f}  {a:8.2f}s  {b:8.1f}s   x{b/a:6.1f}")
print("  -> la propia envolvente del fabricante abarca casi DOS DECADAS")

print("\n=== C. RECTIFICACION: la curva por defecto del firmware vs catalogo ===")
print("  I/In   firmware    banda oficial (rapido - lento)     veredicto")
for m in [1.20, 1.45, 1.60, 2.00, 2.15, 2.55]:
    t = t_salto(m)
    a = t_iec(m, tr_, kr_)
    b = t_iec(m, tl_, kl_)
    bs = f"{a:7.2f} - " + ("no dispara" if b is None else f"{b:8.1f}") + " s"
    if t == 0:
        ver = "FUERA (instantaneo, minimo real %.1f s)" % a
        ts = "INSTANT"
    else:
        ts = f"{t:8.1f} s"
        if b is None:
            ver = "dentro" if t >= a else "por debajo del limite rapido"
        elif t < a:
            ver = f"{a/t:.1f}x por debajo del limite rapido"
        elif t > b:
            ver = f"{t/b:.1f}x por encima del limite lento"
        else:
            ver = "DENTRO de la banda oficial"
    print(f"  {m:.2f} {ts:>10}   {bs:<32} {ver}")

print("\n=== D. Umbral magnetico real para In = 25 A ===")
print(f"  5.39 In = {5.39*25:.0f} A   |   8.80 In = {8.8*25:.0f} A")
print("  -> el disparo instantaneo del firmware a 2.15 In = 53.75 A esta")
print("     entre 2.5x y 4x por debajo del umbral magnetico REAL.")

print("\n=== E. Propuesta: banda completa para In = 25 A, k/tau del catalogo ===")
In, V = 25.0, 230.0
print("  corriente  potencia   rapido      lento     (frio)")
for m in [1.20, 1.45, 1.60, 2.00, 2.55, 3.00]:
    a, b = t_iec(m, tr_, kr_), t_iec(m, tl_, kl_)
    bs = "no dispara" if b is None else f"{b:8.1f} s"
    print(f"  {m*In:6.2f} A  {m*In*V/1000:6.2f} kW  {a:7.2f} s  {bs}")
