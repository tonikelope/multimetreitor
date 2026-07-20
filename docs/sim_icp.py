# -*- coding: utf-8 -*-
"""Replica fiel de computeICP() (multimetreitor.ino:1605) para cuantificar
el comportamiento del modelo ICP en escenarios reales."""
import math

SEG = [1.13, 1.30, 1.45, 1.60, 1.75, 2.00, 2.15]
CURVE_DEF = [2700, 900, 180, 25, 7, 1]
FLOOR = 0.1


def t_salto(mult, curve=CURVE_DEF):
    """Tiempo de disparo interpolado (log) para un multiplo dado."""
    if mult >= 2.15:
        return 0.0
    if mult < 1.13:
        return None
    for i in range(6):
        if mult < SEG[i + 1]:
            x0, x1 = SEG[i], SEG[i + 1]
            y0 = float(curve[i])
            y1 = float(curve[i + 1]) if i < 5 else FLOOR
            frac = (mult - x0) / (x1 - x0)
            if y0 > 0 and y1 > 0:
                return 10 ** (math.log10(y0) + (math.log10(y1) - math.log10(y0)) * frac)
            return y0 + (y1 - y0) * frac
    return None


class ICP:
    def __init__(self, cooldown=600, curve=CURVE_DEF, refresh_ms=1000):
        self.carga = 0.0
        self.cooldown = cooldown
        self.curve = curve
        self.refresh = refresh_ms

    def step(self, mult, dt):
        maxdt = max(1.0, 2.0 * (self.refresh / 1000.0))
        dt = min(dt, maxdt)
        if mult >= 2.15:
            self.carga = 100.0
            return
        if mult < 1.13:
            self.carga -= (100.0 * dt) / self.cooldown
        else:
            t = t_salto(mult, self.curve)
            self.carga += (100.0 * dt) / t if t and t > 0 else 100.0
        self.carga = max(0.0, min(100.0, self.carga))


if __name__ == "__main__":
    print("=== 1. Curva efectiva: tiempo hasta 100% desde frio ===")
    for m in [1.13, 1.2, 1.3, 1.45, 1.6, 1.75, 1.9, 2.0, 2.05, 2.10, 2.14, 2.15, 3.0]:
        t = t_salto(m)
        print(f"  {m:>5.2f}x In -> {'INSTANTANEO (0 s)' if t is None or t == 0 else f'{t:8.2f} s'}")

    print("\n=== 2. Pico de arranque de motor (bomba/aire acondicionado) ===")
    for pico, dur in [(3.0, 0.2), (5.0, 0.1), (2.5, 0.3)]:
        icp = ICP()
        # el PZEM captura el pico en UNA muestra de 1 s
        icp.step(pico, 1.0)
        print(f"  pico {pico}x In durante {dur*1000:.0f} ms capturado en 1 muestra"
              f" -> icpCarga = {icp.carga:.0f}%")
        # tiempo hasta que vuelve por debajo del umbral 75% en reposo (mult<1.13)
        t = 0
        while icp.carga >= 75 and t < 100000:
            icp.step(0.5, 1.0)
            t += 1
        print(f"     tarda {t} s ({t/60:.1f} min) en bajar de 75% con la casa en reposo")

    print("\n=== 3. Aliasing: carga pulsante (vitroceramica/horno con ciclo ON-OFF) ===")
    # Carga que cicla 50% duty a 3.0x / 0.0x con periodo 4 s. Corriente RMS real
    # equivalente = sqrt(0.5*3^2) = 2.12x -> el ICP real dispararia en ~1 s.
    # Se muestrea 1 vez/s: segun la fase, el modelo ve 3.0 o 0.0.
    import itertools
    for fase in (0, 1):
        icp = ICP()
        for s in range(60):
            ciclo = ((s + fase) // 2) % 2  # 2 s ON, 2 s OFF
            icp.step(3.0 if ciclo else 0.0, 1.0)
        print(f"  fase={fase}: tras 60 s de ciclado 3x/0x -> icpCarga = {icp.carga:.0f}%")

    print("\n=== 4. Perdida de tiempo por clamp de dt (loop bloqueado) ===")
    # Sobrecarga sostenida a 1.6x (t_salto = 25 s) con un bloqueo de 10 s
    for bloqueo in [0, 5, 10, 20]:
        icp = ICP()
        icp.step(1.6, 1.0)
        base = icp.carga
        icp2 = ICP()
        icp2.step(1.6, 1.0 + bloqueo)
        real = 100.0 * (1.0 + bloqueo) / 25.0
        print(f"  bloqueo {bloqueo:>2} s a 1.6x: modelo suma {icp2.carga:5.1f}%"
              f" | tiempo real transcurrido = {real:5.1f}% | perdido {real - icp2.carga:5.1f} pp")

    print("\n=== 5. Enfriamiento lineal vs exponencial (bimetalico real) ===")
    icp = ICP()
    icp.carga = 100.0
    tau = 600.0
    print("   t(s)   lineal(600s)   exponencial(tau=600s)")
    for t in [0, 60, 120, 300, 600, 900]:
        lin = max(0.0, 100.0 - 100.0 * t / 600.0)
        exp = 100.0 * math.exp(-t / tau)
        print(f"  {t:>5}   {lin:>8.1f}%      {exp:>8.1f}%")

    print("\n=== 6. Zona ciega 1.00x-1.13x ===")
    # A 1.10x el modelo ENFRIA a plena velocidad (como si estuviera a 0 A).
    icp = ICP()
    icp.carga = 90.0
    for _ in range(300):
        icp.step(1.10, 1.0)
    print(f"  300 s a 1.10x In partiendo de 90%: modelo -> {icp.carga:.1f}%"
          f"  (imagen termica real: se estabiliza en ~{100*1.10**2/1.13**2:.0f}% del equilibrio)")

    print("\n=== 7. Curva 'desde frio': precarga previa ignorada ===")
    # ICP real a 0.95x In durante 1 h esta termicamente casi en equilibrio.
    # El modelo lo cuenta como frio (carga 0).
    icp = ICP()
    for _ in range(3600):
        icp.step(0.95, 1.0)
    print(f"  1 h a 0.95x In -> modelo icpCarga = {icp.carga:.1f}%")
    print(f"  imagen termica: theta = {100*0.95**2/1.13**2:.0f}% del umbral de disparo")
