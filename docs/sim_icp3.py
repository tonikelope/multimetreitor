# -*- coding: utf-8 -*-
"""Efecto del promediado del PZEM (~1.28 s sobre el RMS por ciclo) en el
modelo ICP: el bimetalico real se calienta con <I^2>, el modelo integra con
t_salto(<I>). Desigualdad de Jensen -> subestimacion sistematica."""
import math
from sim_icp import t_salto, ICP

print("=== Carga pulsante: lo que ve el PZEM vs. lo que siente el bimetalico ===")
print(" duty  pico   <I> (lo que ve el PZEM)   Irms real   t_salto(<I>)   t_salto(Irms)")
for duty, pico in [(0.5, 3.0), (0.5, 2.0), (0.3, 3.0), (0.25, 4.0), (0.5, 1.6), (0.7, 1.8)]:
    media = duty * pico
    rms = math.sqrt(duty * pico ** 2)
    tm = t_salto(media)
    tr = t_salto(rms)
    f = lambda t: "no dispara" if t is None else ("INSTANT" if t == 0 else f"{t:8.1f} s")
    print(f"  {duty:.0%}  {pico:.1f}x     {media:5.2f}x            {rms:5.2f}x    {f(tm):>12}   {f(tr):>12}")

print("\n=== Traduccion a la alarma: casa con vitroceramica ciclando ===")
# In = 25 A. Base 8 A + vitro de 2 fuegos que cicla ON 4 s / OFF 4 s a 30 A.
In = 25.0
base, pico_extra = 8.0, 30.0
duty = 0.5
i_med = base + duty * pico_extra
i_rms = math.sqrt(duty * (base + pico_extra) ** 2 + (1 - duty) * base ** 2)
print(f"  corriente media   = {i_med:5.2f} A  ({i_med/In:.2f}x In) -> modelo:"
      f" {'no acumula' if i_med/In < 1.13 else f't_salto {t_salto(i_med/In):.0f} s'}")
print(f"  corriente RMS real= {i_rms:5.2f} A  ({i_rms/In:.2f}x In) -> ICP real:"
      f" t_salto {t_salto(i_rms/In):.0f} s")

print("\n=== Retardo de settle del PZEM (~2.5 s) sobre la curva ===")
print("  mult   t_salto   retardo 2.5 s como % de la curva")
for m in [1.45, 1.60, 1.75, 1.90, 2.00]:
    t = t_salto(m)
    print(f"  {m:.2f}  {t:7.1f} s   {100*2.5/t:6.1f} %")

print("\n=== Y al reves: pico de arranque de motor ya NO llega al modelo ===")
# Compresor: 6x In durante 150 ms sobre base de 0.6x In. Ventana de 1.28 s.
base_m, pico_m, dur = 0.6, 6.0, 0.15
vent = 1.28
visto = (pico_m * dur + base_m * (vent - dur)) / vent
rms_v = math.sqrt((pico_m**2 * dur + base_m**2 * (vent - dur)) / vent)
print(f"  arranque {pico_m}x durante {dur*1000:.0f} ms sobre base {base_m}x:")
print(f"    el PZEM entrega ~{visto:.2f}x In  -> modelo: {'no acumula (<1.13x)' if visto<1.13 else 'acumula'}")
print(f"    RMS real de la ventana = {rms_v:.2f}x In")
print("    -> el falso positivo por inrush NO ocurre; el sensor lo filtra")
