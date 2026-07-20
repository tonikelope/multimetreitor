# -*- coding: utf-8 -*-
"""1) Contrasta la curva POR DEFECTO del firmware con las bandas reales
   documentadas del ICP.  2) Busca la calibracion (tau, k) del modelo
   IEC 60255-149 que cae dentro de todas las bandas."""
import math
from sim_icp import t_salto

# Bandas de tiempo de disparo del ICP real (desde frio).
# Fuente: lecturas publicadas de la curva del ICP (Nergiza) + IEC 60898-1.
BANDAS = {
    1.20: (120, 600),     # 2-10 min
    1.50: (27, 180),      # 27 s - 3 min
    2.00: (10, 90),       # 10 s - 1,5 min
    2.55: (1, 60),        # IEC 60898-1 tabla 7, In <= 32 A  [NORMATIVO]
}

print("=== 1. Curva por defecto del firmware {2700,900,180,25,7,1} vs banda real ===")
print("  mult    modelo      banda real      veredicto")
for m, (lo, hi) in BANDAS.items():
    t = t_salto(m)
    if t is None:
        v, ts = "NO ACUMULA", "-"
    elif t == 0:
        v, ts = f">={hi:.0f}x DEMASIADO RAPIDO", "INSTANT"
    else:
        ts = f"{t:8.1f} s"
        if t < lo:
            v = f"{lo/t:5.1f}x DEMASIADO RAPIDO"
        elif t > hi:
            v = f"{t/hi:5.1f}x DEMASIADO LENTO"
        else:
            v = "dentro de banda"
    print(f"  {m:.2f}  {ts:>10}   [{lo:5.0f}, {hi:5.0f}] s   {v}")


print("\n=== 2. Modelo IEC 60255-149: t(I) = tau * ln(I^2 / (I^2 - (k)^2)) ===")


def t_iec(mult, tau, k):
    if mult <= k:
        return None
    return tau * math.log(mult ** 2 / (mult ** 2 - k ** 2))


mejor = None
for k10 in range(105, 131):
    k = k10 / 100.0
    # tau que satisface cada banda
    lo_t, hi_t = 0.0, 1e9
    ok = True
    for m, (lo, hi) in BANDAS.items():
        f = t_iec(m, 1.0, k)
        if f is None:
            ok = False
            break
        lo_t = max(lo_t, lo / f)
        hi_t = min(hi_t, hi / f)
    if ok and lo_t <= hi_t:
        span = hi_t / lo_t
        print(f"  k = {k:.2f}  ->  tau admisible [{lo_t:7.1f}, {hi_t:7.1f}] s"
              f"   (holgura x{span:.2f})")
        if mejor is None or span > mejor[2]:
            mejor = (k, math.sqrt(lo_t * hi_t), span, lo_t, hi_t)

k, tau, span, lo_t, hi_t = mejor
print(f"\n  MEJOR AJUSTE: k = {k:.2f} In, tau = {tau:.0f} s"
      f"  (rango valido {lo_t:.0f}-{hi_t:.0f} s, holgura x{span:.1f})")

print("\n=== 3. Curva resultante y comprobacion punto a punto ===")
print("  mult   t modelo IEC    banda real        ok")
for m in [1.13, 1.20, 1.30, 1.45, 1.50, 1.60, 1.75, 2.00, 2.15, 2.55, 3.00, 5.00]:
    t = t_iec(m, tau, k)
    b = BANDAS.get(m)
    ts = "no dispara" if t is None else f"{t:8.1f} s"
    if b:
        marca = "OK" if b[0] <= t <= b[1] else "FUERA"
        bs = f"[{b[0]:5.0f}, {b[1]:5.0f}] s"
    else:
        marca, bs = "", ""
    print(f"  {m:.2f}  {ts:>12}   {bs:>16}   {marca}")

print("\n=== 4. Efecto del precalentamiento (curva caliente, ec. 6 IEC) ===")
print("  carga previa    t disparo a 2.00x In    vs frio")
tf = t_iec(2.0, tau, k)
for ip in [0.0, 0.5, 0.8, 0.9, 1.0, 1.10]:
    num = 2.0 ** 2 - ip ** 2
    den = 2.0 ** 2 - k ** 2
    t = tau * math.log(num / den)
    print(f"    {ip:.2f}x In         {t:8.1f} s            {100*t/tf:5.0f} %")

print("\n=== 5. Valores para los 6 puntos de la tabla configurable actual ===")
print("  (si se conserva la UI de 6 tramos, estos serian los valores por")
print("   defecto coherentes con el ICP real, en vez de 2700/900/180/25/7/1)")
for m in [1.13, 1.30, 1.45, 1.60, 1.75, 2.00, 2.15]:
    t = t_iec(m, tau, k)
    print(f"    {m:.2f}x  ->  {'infinito' if t is None else f'{t:7.0f} s'}")

print("\n=== 6. Enfriamiento con la misma tau (tau2 = tau1, bimetal pasivo) ===")
print("   t(s)   carga restante desde 100%")
for t in [0, 60, 120, 300, 600, 900, 1200]:
    print(f"  {t:>5}   {100*math.exp(-t/tau):6.1f} %")
print(f"  vuelta practica a frio (5 tau) = {5*tau/60:.1f} min")
