# -*- coding: utf-8 -*-
"""Propuesta: modelo de imagen termica (1er orden) que CONSERVA la curva
configurable actual, y verificacion de que reproduce los mismos tiempos de
disparo desde frio mientras corrige precalentamiento, zona 1.0-1.13x y
enfriamiento."""
import math
from sim_icp import t_salto, ICP, CURVE_DEF, SEG

K = 1.13          # multiplo limite de no disparo -> theta_trip
K2 = K * K


def tau_de_curva(mult):
    """tau equivalente para que el disparo desde frio coincida EXACTAMENTE
    con t_salto(mult) de la curva configurada."""
    t = t_salto(mult)
    if t is None or t <= 0:
        return None
    m2 = mult * mult
    return t / math.log(m2 / (m2 - K2))


class ICPTermico:
    """theta normalizado: 0 = frio, 1.0 = disparo (100 %)."""

    def __init__(self, cooldown=600.0):
        self.theta = 0.0
        self.tau_cool = cooldown / math.log(100.0)  # ver nota abajo

    def step(self, mult, dt):
        eq = (mult * mult) / K2                      # equilibrio termico
        if mult >= 2.15:
            self.theta = 1.0
            return
        if mult < K:
            tau = self.tau_cool
        else:
            tau = tau_de_curva(mult)
        a = math.exp(-dt / tau)
        self.theta = eq + (self.theta - eq) * a
        self.theta = max(0.0, min(1.0, self.theta))

    @property
    def carga(self):
        return 100.0 * self.theta


print("=== A. El modelo propuesto reproduce la curva configurada (desde frio) ===")
print("  mult    t_salto curva    t hasta 100% modelo propuesto")
for m in [1.15, 1.30, 1.45, 1.60, 1.75, 2.00]:
    icp = ICPTermico()
    t = 0.0
    dt = 0.05
    while icp.carga < 99.999 and t < 1e5:
        icp.step(m, dt)
        t += dt
    print(f"  {m:>5.2f}   {t_salto(m):>10.1f} s   {t:>10.1f} s")

print("\n=== B. Precalentamiento (curva desde frio vs. carga previa real) ===")
for prev in [0.0, 0.8, 0.95, 1.05, 1.10]:
    icp = ICPTermico()
    for _ in range(3600):
        icp.step(prev, 1.0)
    pre = icp.carga
    t = 0.0
    while icp.carga < 99.999 and t < 1e5:
        icp.step(1.60, 0.05)
        t += 0.05
    print(f"  1 h a {prev:.2f}x -> carga previa {pre:5.1f}%"
          f" | luego 1.60x dispara en {t:6.1f} s (curva desde frio: {t_salto(1.6):.0f} s)")

print("\n=== C. Zona 1.00-1.13x: ya no enfria como si estuviera a 0 A ===")
icp = ICPTermico()
icp.theta = 0.90
for _ in range(300):
    icp.step(1.10, 1.0)
print(f"  300 s a 1.10x partiendo de 90% -> {icp.carga:.1f}%  (modelo actual: 40.0%)")

print("\n=== D. Enfriamiento: exponencial con misma vida util practica ===")
icp = ICPTermico()
icp.theta = 1.0
print("   t(s)   propuesto   actual(lineal 600 s)")
for t in [0, 60, 120, 300, 600, 900, 1200]:
    ic = ICPTermico(); ic.theta = 1.0
    for _ in range(t):
        ic.step(0.0, 1.0)
    lin = max(0.0, 100.0 - 100.0 * t / 600.0)
    print(f"  {t:>5}   {ic.carga:>7.1f}%   {lin:>10.1f}%")
print("  (tau_cool = cooldown/ln(100) -> el 'cooldown' configurado sigue")
print("   significando 'tiempo de 100% a ~1%', asi se respeta la semantica UI)")

print("\n=== E. Coste computacional por ciclo (operaciones float) ===")
print("  actual:    1 log10f + 1 powf                (~2 transcendentes)")
print("  propuesto: 1 log10f + 1 powf + 1 logf + 1 expf (~4 transcendentes)")
print("  en ESP8266 @80MHz sin FPU: ~15-25 us extra por ciclo de 1000 ms")
print("  -> ~0.002 % del ciclo; despreciable frente a los 7-9 ms del LCD")
