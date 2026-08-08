"""
Validate STANAG 4355 against FT 155-AM-2 (M107 HE)
Using G1 drag curve with fixed cd0 (calibrated from Argentine real fire)
"""

import csv
import math

# FT 155-AM-2 v0 values for M109 (from TM 9-2350-217-10 / War Thunder data)
# These are the STANDARD muzzle velocities for each charge
M109_V0 = {
    '3G': 275.8,
    '4G': 317.0,
    '5G': 374.9,
    '3W': 269.7,
    '4W': 313.9,
    '5W': 373.4,
    '6W': 461.8,  # FT baseline v0 for 6W
    '7W': 562.4,
    '8':  682.0,  # M119A1 charge 8
}

# Our calibrated v0 (from Argentine real fire)
OUR_V0 = {
    '3G': 279.0,
    '4G': 320.0,
    '5G': 382.0,
    '3W': 292.0,
    '4W': 334.0,
    '5W': 389.0,
    '6W': 495.0,  # Calibrated from real fire
    '7W': 565.0,
    '8':  827.0,
}

# G1 STANDARD PROJECTILE DRAG CURVE
# Cd vs Mach number (from McCoy / STANAG)
G1_TABLE = [
    (0.00, 0.2300), (0.20, 0.2300), (0.40, 0.2300), (0.60, 0.2350),
    (0.70, 0.2500), (0.80, 0.2800), (0.85, 0.3100), (0.90, 0.3500),
    (0.95, 0.4000), (1.00, 0.4500), (1.05, 0.4700), (1.10, 0.4700),
    (1.15, 0.4600), (1.20, 0.4500), (1.30, 0.4300), (1.40, 0.4100),
    (1.50, 0.3900), (1.60, 0.3750), (1.70, 0.3600), (1.80, 0.3500),
    (1.90, 0.3400), (2.00, 0.3300), (2.20, 0.3150), (2.50, 0.3000),
    (3.00, 0.2600),
]

G1_CD0_REF = 0.2300  # Cd at M=0 for G1 standard

def interp_g1_cd(mach):
    """Interpolate G1 drag coefficient"""
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

def solve_stanag(dist_m, v0, cd0=0.157):
    """Solve for QE using STANAG 4355 model with G1 drag and fixed cd0"""
    mass = 43.2  # kg
    caliber = 0.155  # m
    area = math.pi * (caliber/2)**2  # m²
    
    # Atmosphere (ISA standard)
    def atmosphere(h):
        T = 288.15 - 0.0065 * h
        P = 101325 * (T / 288.15) ** 5.2561
        rho = P / (287.05 * T)
        a = math.sqrt(1.4 * 287.05 * T)
        return rho, a
    
    def trajectory(qe_mil):
        """Compute range for given QE (mils)"""
        qe_rad = qe_mil * (math.pi / 3200.0)
        
        # Initial conditions
        vx = v0 * math.cos(qe_rad)
        vy = v0 * math.sin(qe_rad)
        x = 0.0
        y = 0.0
        
        dt = 0.005  # seconds
        
        for _ in range(20000):  # max 100 seconds
            if y < 0 and x > 0:
                break
            
            rho, a = atmosphere(y)
            v = math.sqrt(vx**2 + vy**2)
            mach = v / a if a > 0 else 0
            
            # G1 drag: cd = cd0 * (G1_cd / G1_cd0_ref)
            cd_g1 = interp_g1_cd(mach)
            cd = cd0 * (cd_g1 / G1_CD0_REF)
            
            # Drag force
            drag = 0.5 * rho * v**2 * cd * area
            drag_x = -drag * (vx / v) if v > 0 else 0
            drag_y = -drag * (vy / v) if v > 0 else 0
            
            # Acceleration
            ax = drag_x / mass
            ay = -9.81 + drag_y / mass
            
            # Euler integration
            vx += ax * dt
            vy += ay * dt
            x += vx * dt
            y += vy * dt
        
        return x  # range in meters
    
    # Binary search for QE
    qe_low = 1.0
    qe_high = 1200.0
    
    # Find max range first to determine search direction
    for _ in range(30):
        qe_mid = (qe_low + qe_high) / 2.0
        range_mid = trajectory(qe_mid)
        
        if range_mid < dist_m:
            qe_low = qe_mid
        else:
            qe_high = qe_mid
    
    return (qe_low + qe_high) / 2.0

def load_ft_data():
    """Load FT data from CSV"""
    data = {}
    with open('tables/tables.csv', 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row['ART'] == '155' and row['PROJ'] == 'HEA':
                chg = row['CHG']
                dist = int(row['DIST'])
                qe = float(row['QE'])
                tof = float(row['TOF'])
                if chg not in data:
                    data[chg] = []
                data[chg].append((dist, qe, tof))
    return data

def main():
    print("=" * 70)
    print("STANAG 4355 VALIDATION vs FT 155-AM-2 (M107 HE)")
    print("=" * 70)
    print()
    print("FT data: C1 only (M107), NOT M483A1")
    print("V0: US M109 standard values from TM 9-2350-217-10")
    print("cd0: 0.157 (calibrated from Argentine real fire)")
    print()
    
    ft_data = load_ft_data()
    
    # Test points: sample from each charge
    test_charges = ['3G', '4G', '5G', '3W', '4W', '5W', '6W', '7W']
    
    all_errors = []
    
    for chg in test_charges:
        if chg not in ft_data:
            continue
        if chg not in M109_V0:
            continue
        
        v0 = M109_V0[chg]
        rows = sorted(ft_data[chg], key=lambda x: x[0])
        
        # Sample 5 points from middle range
        n = len(rows)
        sample_indices = [int(n * p) for p in [0.2, 0.4, 0.6, 0.8, 0.95]]
        sample_indices = [min(i, n-1) for i in sample_indices]
        
        print(f"\n--- {chg} (v0={v0:.1f} m/s) ---")
        print(f"{'DIST':>6} {'FT_QE':>7} {'STN_QE':>7} {'ERROR':>7} {'ERR%':>6}")
        print("-" * 40)
        
        chg_errors = []
        for idx in sample_indices:
            dist, ft_qe, ft_tof = rows[idx]
            stn_qe = solve_stanag(dist, v0, cd0=0.157)
            error = stn_qe - ft_qe
            pct = (error / ft_qe) * 100 if ft_qe > 0 else 0
            
            chg_errors.append(error)
            all_errors.append(error)
            
            print(f"{dist:6d} {ft_qe:7.1f} {stn_qe:7.1f} {error:+7.1f} {pct:+6.1f}%")
        
        rmse = math.sqrt(sum(e**2 for e in chg_errors) / len(chg_errors))
        print(f"  RMSE: {rmse:.1f} mils")
    
    # Overall statistics
    print("\n" + "=" * 70)
    print("OVERALL VALIDATION")
    print("=" * 70)
    
    if all_errors:
        rmse_all = math.sqrt(sum(e**2 for e in all_errors) / len(all_errors))
        max_err = max(abs(e) for e in all_errors)
        mean_err = sum(all_errors) / len(all_errors)
        mean_abs = sum(abs(e) for e in all_errors) / len(all_errors)
        
        print(f"\nTotal test points: {len(all_errors)}")
        print(f"RMSE:           {rmse_all:.1f} mils")
        print(f"Mean Error:     {mean_err:+.1f} mils")
        print(f"Mean Abs Error: {mean_abs:.1f} mils")
        print(f"Max Error:      {max_err:.1f} mils")
        
        print("\n--- Comparison with Argentine real fire ---")
        print(f"Real fire (6W @ 10,458m): QE = 428.9 mils")
        print(f"STANAG (6W @ 10,467m):   QE = 428.9 mils (cd0=0.157, v0=495)")
        print(f"FT (6W @ 10,453m):       QE = 470.0 mils (M107)")
        print()
        print("NOTE: Argentine CITER L33 has HIGHER v0 than M109/M777")
        print("      for the same charge (different propellant formulation)")
        print("      So FT values (M109 v0) will NOT match CITER L33 real fire")

if __name__ == '__main__':
    main()
