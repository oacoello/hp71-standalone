"""
Cálculo completo de tiro con datos reales
BATERIA BRAVO 155mm 1BAC
"""
import math

# =============================================
# DATOS REALES
# =============================================

# Gun Base (GB)
GB_E = 55311    # Este (m)
GB_N = 83242    # Norte (m)
GB_ALT = 1517   # Altitud (m)

# Target (TGT)
TGT_E = 54069   # Este (m)
TGT_N = 72858   # Norte (m)
TGT_ALT = 1360  # Altitud (m)

# Datos de tiro
AZ_LAY = 3320   # Azimuth de puntería (mils)
QE_REAL = 428.9 # QE real medido (mils)
CHG = '6W'      # Carga

# =============================================
# CÁLCULO DE DISTANCIA Y AZIMUTH
# =============================================

# Diferencias
dE = TGT_E - GB_E   # Este
dN = TGT_N - GB_N   # Norte
dH = TGT_ALT - GB_ALT  # Altitud (negativo = target abajo)

# Distancia horizontal
dist_horiz = math.sqrt(dE**2 + dN**2)

# Distancia 3D
dist_3d = math.sqrt(dE**2 + dN**2 + dH**2)

# Azimuth (desde el Norte, en radianes)
az_rad = math.atan2(dE, dN)
if az_rad < 0:
    az_rad += 2 * math.pi

# Azimuth en mils (1 circle = 6400 mils)
az_mils = az_rad * 6400 / (2 * math.pi)

# Ángulo de elevación del blanco (negativo = targets abajo)
elev_rad = math.atan2(-dH, dist_horiz)
elev_mils = elev_rad * 3200 / math.pi

print("=" * 70)
print("CÁLCULO COMPLETO DE TIRO — BATERIA BRAVO 155mm 1BAC")
print("=" * 70)
print()
print("COORDENADAS:")
print(f"  Gun Base:  E={GB_E}  N={GB_N}  ALT={GB_ALT}m")
print(f"  Target:    E={TGT_E}  N={TGT_N}  ALT={TGT_ALT}m")
print()
print("DIFERENCIAS:")
print(f"  dE = {dE:+d}m (Este)")
print(f"  dN = {dN:+d}m (Norte)")
print(f"  dH = {dH:+d}m (Altitud)")
print()
print("DISTANCIA:")
print(f"  Horizontal: {dist_horiz:.1f}m")
print(f"  3D:         {dist_3d:.1f}m")
print()
print("AZIMUTH:")
print(f"  Calculado:  {az_mils:.1f} mils")
print(f"  De puntería: {AZ_LAY} mils")
print(f"  Diferencia:  {az_mils - AZ_LAY:+.1f} mils")
print()
print("ELEVACIÓN DEL BLANCO:")
print(f"  {elev_mils:.1f} mils ({math.degrees(elev_rad):+.2f}°)")
print()

# =============================================
# MODELO BALÍSTICO G1 (STANAG 4355)
# =============================================

# G1 drag table
G1_TABLE = [
    (0.00, 0.2300), (0.20, 0.2300), (0.40, 0.2300), (0.60, 0.2350),
    (0.70, 0.2500), (0.80, 0.2800), (0.85, 0.3100), (0.90, 0.3500),
    (0.95, 0.4000), (1.00, 0.4500), (1.05, 0.4700), (1.10, 0.4700),
    (1.15, 0.4600), (1.20, 0.4500), (1.30, 0.4300), (1.40, 0.4100),
    (1.50, 0.3900), (1.60, 0.3750), (1.70, 0.3600), (1.80, 0.3500),
    (1.90, 0.3400), (2.00, 0.3300), (2.20, 0.3150), (2.50, 0.3000),
    (3.00, 0.2600),
]
G1_CD0_REF = 0.2300

def interp_g1_cd(mach):
    if mach <= G1_TABLE[0][0]:
        return G1_TABLE[0][1]
    if mach >= G1_TABLE[-1][0]:
        return G1_TABLE[-1][1]
    for i in range(len(G1_TABLE)-1):
        m0, c0 = G1_TABLE[i]
        m1, c1 = G1_TABLE[i+1]
        if m0 <= mach <= m1:
            t = (mach - m0) / (m1 - m0)
            return c0 + t * (c1 - c0)
    return G1_TABLE[-1][1]

# Projectile data
MASS = 43.2      # kg (M107 HE)
CALIBER = 0.155  # m
AREA = math.pi * (CALIBER/2)**2  # m²
CD0 = 0.157      # calibrated from Argentine real fire

# Atmosphere (ISA)
def atmosphere(h):
    T = 288.15 - 0.0065 * max(h, 0)
    P = 101325 * (T / 288.15) ** 5.2561
    rho = P / (287.05 * T)
    a = math.sqrt(1.4 * 287.05 * T)
    return rho, a

def trajectory(qe_mil, v0, target_dist, target_alt):
    """Compute range for given QE (mils) — returns (range, altitude)"""
    qe_rad = qe_mil * (math.pi / 3200.0)
    
    vx = v0 * math.cos(qe_rad)
    vy = v0 * math.sin(qe_rad)
    x = 0.0
    y = 0.0
    
    dt = 0.005
    
    for _ in range(40000):  # max 200 seconds
        # Check if projectile hit ground
        if y < 0 and x > 0:
            break
        
        rho, a = atmosphere(y)
        v = math.sqrt(vx**2 + vy**2)
        mach = v / a if a > 0 else 0
        
        cd_g1 = interp_g1_cd(mach)
        cd = CD0 * (cd_g1 / G1_CD0_REF)
        
        drag = 0.5 * rho * v**2 * cd * AREA
        drag_x = -drag * (vx / v) if v > 0 else 0
        drag_y = -drag * (vy / v) if v > 0 else 0
        
        ax = drag_x / MASS
        ay = -9.81 + drag_y / MASS
        
        vx += ax * dt
        vy += ay * dt
        x += vx * dt
        y += vy * dt
    
    return x, y

def solve_qe(v0, target_dist, target_alt):
    """Binary search for QE to hit target at given distance and altitude"""
    # First find approximate QE by scanning
    best_qe = 1.0
    best_dist = 0.0
    
    for qe_test in range(1, 600, 5):  # scan 1-600 mils in 5 mil steps
        x, y = trajectory(qe_test, v0, target_dist, target_alt)
        if x > best_dist:
            best_dist = x
            best_qe = qe_test
    
    print(f"  Scan: max range at QE={best_qe} mils = {best_dist:.0f}m")
    
    # Now binary search around the target distance
    if best_dist < target_dist:
        print(f"  WARNING: Max range ({best_dist:.0f}m) < target ({target_dist:.0f}m)")
        return best_qe
    
    # Binary search on ascending side (QE < best_qe)
    qe_low = 1.0
    qe_high = best_qe
    
    for _ in range(50):
        qe_mid = (qe_low + qe_high) / 2.0
        x, y = trajectory(qe_mid, v0, target_dist, target_alt)
        
        if x < target_dist:
            qe_low = qe_mid
        else:
            qe_high = qe_mid
    
    return (qe_low + qe_high) / 2.0

# V0 for 6W (calibrated from Argentine real fire)
V0_6W = 495.0  # m/s

print("MODELO BALÍSTICO G1 (STANAG 4355):")
print(f"  cd0:     {CD0}")
print(f"  v0 6W:   {V0_6W} m/s")
print(f"  masa:    {MASS} kg")
print(f"  calibre: {CALIBER*1000:.0f} mm")
print()

# Solve for QE
print("RESOLVIENDO QE...")
qe_calc = solve_qe(V0_6W, dist_horiz, TGT_ALT - GB_ALT)
print()

# =============================================
# RESULTADOS
# =============================================
print("=" * 70)
print("RESULTADOS")
print("=" * 70)
print()
print(f"  Distancia horizontal: {dist_horiz:.1f}m")
print(f"  Diferencia altitud:   {dH:+d}m")
print(f"  Azimuth calculado:    {az_mils:.1f} mils")
print(f"  Azimuth de puntería:  {AZ_LAY} mils")
print()
print(f"  QE REAL:      {QE_REAL:.1f} mils")
print(f"  QE CALCULADO: {qe_calc:.1f} mils")
print(f"  DIFERENCIA:   {qe_calc - QE_REAL:+.1f} mils")
print(f"  ERROR %:      {((qe_calc - QE_REAL) / QE_REAL * 100):+.1f}%")
print()
print("NOTA: El QE calculado usa el modelo G1 con cd0=0.157")
print("      calibrado contra el fuego real argentino.")
print("      La diferencia puede deberse a:")
print("      - V0 real desconocido (usamos 495 m/s estimado)")
print("      - Condiciones atmosféricas reales vs ISA")
print("      - Viento no considerado")
print("      - Error de medición del QE real")
