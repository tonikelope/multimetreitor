# -*- coding: utf-8 -*-
"""Replica EXACTA de computeICP()+evaluateAlerts() tal como quedan en el
firmware, para validar el comportamiento antes de flashear."""
import math
K, TAU, TAU2, IN = 1.30, 246.0, 246.0, 25.0
NEVER_TRIP = 1.13
TRIG_SAMPLES, HYST, LEAD = 3, 10.0, 120.0

class ICP:
    def __init__(self, umbral=75):
        self.h = 0.0          # icpCarga en %
        self.umbral = umbral
        self.latch = False
        self.fast = 0
    def cycle(self, amps, dt=1.0):
        mult = amps / IN
        heq = 100.0 * (mult*mult)/(K*K)
        tau = TAU if mult > 0.05 else TAU2
        self.h = heq + (self.h - heq)*math.exp(-dt/tau)
        self.h = max(0.0, min(100.0, self.h))
        armed = mult > NEVER_TRIP
        clear_at = max(1.0, self.umbral - HYST)
        heqn = (mult*mult)/(K*K)
        imminent = False
        if heqn > 1.0:
            h_n = self.h/100.0
            left = 0.0 if h_n >= 1.0 else TAU*math.log((heqn-h_n)/(heqn-1.0))
            imminent = left <= LEAD
        if imminent:
            self.fast = min(TRIG_SAMPLES, self.fast+1)
        else:
            self.fast = 0
        if not armed:
            self.latch = False
        elif self.fast >= TRIG_SAMPLES or self.h >= self.umbral:
            self.latch = True
        elif self.h < clear_at:
            self.latch = False
        return self.latch

def run(nombre, tramos, umbral=75):
    icp = ICP(umbral); t = 0; t_alarm = None; hmax = 0
    for amps, dur in tramos:
        for _ in range(int(dur)):
            a = icp.cycle(amps); t += 1; hmax = max(hmax, icp.h)
            if a and t_alarm is None: t_alarm = t
    estado = "ALARMA" if icp.latch else "sin alarma"
    aviso = f"aviso a los {t_alarm} s" if t_alarm else "nunca avisó"
    print(f"  {nombre:44} carga {icp.h:5.1f}% (max {hmax:5.1f}%)  {estado:10} {aviso}")

print("=== A. Falsas alarmas en uso cotidiano (umbral 75, el de fabrica) ===")
run("Reposo 3 A durante 24 h",            [(3.0, 86400)])
run("15 A (3,4 kW) sostenidos 2 h",       [(15.0, 7200)])
run("20 A (4,6 kW) sostenidos 2 h",       [(20.0, 7200)])
run("25 A = limite contratado, 3 h",      [(25.0, 10800)])
run("28 A (6,4 kW) sostenidos 2 h",       [(28.0, 7200)])

print("\n=== B. Con TU umbral actual (40 %) ===")
run("Reposo 3 A durante 24 h",            [(3.0, 86400)], 40)
run("20 A sostenidos 2 h",                [(20.0, 7200)], 40)
run("25 A sostenidos 3 h",                [(25.0, 10800)], 40)
run("28 A sostenidos 1 h",                [(28.0, 3600)], 40)

print("\n=== C. Sobrecargas reales: ¿avisa a tiempo? ===")
run("Frio -> 50 A (11,5 kW)",             [(3.0, 600), (50.0, 300)])
run("Frio -> 64 A (14,7 kW)",             [(3.0, 600), (64.0, 200)])
run("Casa a 25 A 1 h -> 40 A",            [(25.0, 3600), (40.0, 300)])
run("Casa a 25 A 1 h -> 34 A (zona gris)",[(25.0, 3600), (34.0, 600)])
run("29 A sostenidos 1 h (zona 1.13-1.30)",[(29.0, 3600)])

print("\n=== D. Transitorios que NO deben avisar ===")
run("Pico de 40 A durante 2 s",           [(3.0, 600), (40.0, 2), (3.0, 300)])
run("Arranque: 35 A 5 s cada 10 min x6",  [(3.0,600)] + [(35.0,5),(3.0,595)]*6)

print("\n=== E. Recuperacion tras la alarma (histeresis) ===")
icp = ICP(75)
for _ in range(600): icp.cycle(3.0)
for _ in range(200): icp.cycle(50.0)
print(f"  tras 200 s a 50 A: carga {icp.h:.1f}% alarma={icp.latch}")
n = 0
while icp.latch and n < 20000:
    icp.cycle(3.0); n += 1
print(f"  vuelve a 3 A -> alarma se apaga en {n} s ({n/60:.1f} min), carga {icp.h:.1f}%")
