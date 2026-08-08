"""
Convert FT155 and FT105 firing tables Excel -> CSV (v7)

FIXES:
  - FT155: TOF is column 6 (C5 was wind correction in meters!)
  - FT155: Uses C1 (QE_M107) ONLY, NOT C1+C2 (M483A1)
    HEA = M107 projectile, so we store M107 values
    C2 is the correction for M483A1 which is a DIFFERENT projectile
  - FT105: Post-processing enforces QE monotonicity for low-angle
  - FT105: Stricter classification of interleaved entries

FT155 columns: C1=QE_M107, C2=CORR_M483A1, C3=CORR_50mHGT, C4=CORR_100mRG,
  C5=CORR_WIND(m), C6=TOF(sec), C7=RANGE(m), C8=CORR_DEFL, C9=DRIFT, C10=TV

IMPORTANT: FT155-AM-2 is for US Army guns (M109/M198/M777, L39-40 barrels)
  NOT for Argentine CITER L33 (L33 barrel, derived from French SOFMA/AMX MK F3)
  The L33 has different v0 per charge than US guns.

FT105 columns: C2=QE, C3=TOF(sec), C4=RANGE(m), C5=ANGLE(deg),
  C6=DRIFT, C7=TV(m/s), C8=CORR_WIND, C9=CORR_HGT, C10=CORR_RG
"""

import openpyxl
import os
import csv

FT155_PATH = r'C:\Users\DolfoZR\OneDrive\Descargas PC\DESCARGAS_JUL2026\FT155_ADD_R1_M483A1_Firing_Tables_with_DRIFT.xlsx'
FT105_PATH = r'C:\Users\DolfoZR\OneDrive\Descargas PC\DESCARGAS_JUL2026\FT105_ADD_R1_M102_Firing_Tables.xlsx'
OUTPUT_PATH = r'tables\tables.csv'


def parse_drift(val):
    if val is None: return 0.0
    if isinstance(val, (int, float)): return float(val)
    s = str(val).strip().upper()
    if not s: return 0.0
    if s.startswith('R'):
        try: return float(s[1:])
        except: return 0.0
    elif s.startswith('L'):
        try: return -float(s[1:])
        except: return 0.0
    try: return float(s)
    except: return 0.0


# =================== FT155 ===================

def extract_ft155(wb):
    rows = []
    charge_map = {
        'Charge_3G_TableA': '3G', 'Charge_3W_TableA': '3W',
        'Charge_4G_TableA': '4G', 'Charge_4W_TableA': '4W',
        'Charge_5G_TableA': '5G', 'Charge_5W_TableA': '5W',
        'Charge_6W_TableA': '6W', 'Charge_7W_TableA': '7W',
        'Charge_8_TableA':  '8',
    }
    for sheet, chg in charge_map.items():
        if sheet not in wb.sheetnames: continue
        ws = wb[sheet]
        count = 0
        for r in range(5, ws.max_row + 1):
            qe_m107 = ws.cell(r, 1).value   # C1: QE for M107
            corr    = ws.cell(r, 2).value    # C2: Correction for M483A1
            tof     = ws.cell(r, 6).value    # C6: TOF (seconds) *** FIXED ***
            dist    = ws.cell(r, 7).value    # C7: Range (meters)
            drift   = ws.cell(r, 9).value    # C9: Drift (mils)
            if qe_m107 is None or dist is None: continue
            if not isinstance(dist, (int, float)) or dist <= 0: continue
            # Use C1 only (M107 HE), NOT C1+C2 (M483A1)
            # HEA = M107 projectile, so we store M107 QE values
            # C2 is the correction for M483A1 which is a DIFFERENT projectile
            qe = float(qe_m107)
            rows.append({
                'art': 155, 'proj': 'HEA', 'chg': chg,
                'dist': int(dist), 'qe': round(qe, 1),
                'tof': round(float(tof) if tof and isinstance(tof, (int, float)) else 0.0, 1),
                'drift': round(parse_drift(drift), 1),
                'angle': 0.0, 'vterm': 0.0,
            })
            count += 1
        print("  {} -> CHG {}: {} rows".format(sheet, chg, count))
    return rows


# =================== FT105 ===================

def extract_ft105(wb):
    all_rows = []
    charge_map = {
        'Charge_1_TableA': '1', 'Charge_2_TableA': '2', 'Charge_3_TableA': '3',
        'Charge_4_TableA': '4', 'Charge_5_TableA': '5', 'Charge_6_TableA': '6',
        'Charge_7_TableA': '7',
    }
    
    for sheet_name, chg in charge_map.items():
        if sheet_name not in wb.sheetnames: continue
        ws = wb[sheet_name]
        
        raw = []
        for r in range(5, ws.max_row + 1):
            qe    = ws.cell(r, 2).value
            tof   = ws.cell(r, 3).value
            dist  = ws.cell(r, 4).value
            angle = ws.cell(r, 5).value
            drift = ws.cell(r, 6).value
            vterm = ws.cell(r, 7).value
            if qe is None or dist is None: continue
            if not isinstance(dist, (int, float)) or dist <= 0: continue
            raw.append({
                'art': 105, 'proj': 'HEA', 'chg': chg,
                'dist': int(dist),
                'qe': round(float(qe), 1),
                'tof': round(float(tof) if tof and isinstance(tof, (int, float)) else 0.0, 1),
                'angle': round(float(angle) if angle and isinstance(angle, (int, float)) else 0.0, 1),
                'drift': round(parse_drift(drift), 1),
                'vterm': round(float(vterm) if vterm and isinstance(vterm, (int, float)) else 0.0, 1),
            })
        
        if not raw: continue
        
        # === Find clean low-angle section ===
        # Criteria: QE monotonic, no jumps > 30%, angle < 35, vterm > 150
        clean_end = 0
        prev_qe = -1
        for i, row in enumerate(raw):
            qe_ok = row['qe'] >= prev_qe - 0.5
            qe_jump_ok = (prev_qe <= 0) or (row['qe'] <= prev_qe * 1.3)
            vterm_ok = row['vterm'] > 150 or i < 3
            angle_ok = row['angle'] < 35 or i < 3
            
            if qe_ok and qe_jump_ok and vterm_ok and angle_ok:
                clean_end = i + 1
                prev_qe = row['qe']
            else:
                break
        
        clean = raw[:clean_end]
        last_clean_qe = clean[-1]['qe'] if clean else 89
        last_clean_dist = clean[-1]['dist'] if clean else 0
        
        # === Compute linear extrapolation from clean section ===
        if len(clean) >= 2:
            n = len(clean)
            xs = [e['dist'] for e in clean]
            ys = [e['qe'] for e in clean]
            sum_x = sum(xs)
            sum_x2 = sum(x**2 for x in xs)
            denom = n * sum_x2 - sum_x**2
            if abs(denom) > 1e-6:
                slope_qe = (n * sum(x*y for x,y in zip(xs, ys)) - sum_x * sum(ys)) / denom
                intcpt_qe = (sum(ys) - slope_qe * sum_x) / n
            else:
                slope_qe = 0
                intcpt_qe = ys[-1]
        else:
            slope_qe = 0
            intcpt_qe = clean[-1]['qe'] if clean else 89
        
        # === Classify remaining entries ===
        low_extra = []
        high_rows = []
        last_low_qe = last_clean_qe
        
        for row in raw[clean_end:]:
            qe = row['qe']
            dist = row['dist']
            angle = row['angle']
            vterm = row['vterm']
            
            # Expected QE from clean-section extrapolation
            expected_qe = max(intcpt_qe + slope_qe * dist, last_clean_qe)
            
            is_low = False
            
            # Primary: QE must increase from last low-angle value
            if qe > last_low_qe - 0.5:
                # Hard NO: definitely high-angle
                if angle > 40:
                    is_low = False
                elif vterm < 150:
                    is_low = False
                # QE consistency: must be within 2x of expected trajectory
                elif expected_qe > 0 and qe > expected_qe * 2.0:
                    is_low = False
                # QE jump: must not jump more than 3x from last low-angle value
                elif last_low_qe > 0 and qe > last_low_qe * 3.0:
                    is_low = False
                else:
                    is_low = True
            else:
                is_low = False
            
            if is_low:
                low_extra.append(row)
                last_low_qe = qe
            else:
                high_rows.append(row)
        
        # Combine clean + validated extra — ONLY low-angle rows
        low_rows = clean + low_extra
        
        # Dedup by distance (prefer low-angle, though each dist should be unique)
        low_rows.sort(key=lambda x: x['dist'])
        seen = set()
        deduped = []
        for row in low_rows:
            d = row['dist']
            if d not in seen:
                seen.add(d)
                deduped.append(row)
        
        # Output HEA rows (for lot=A combined match) and HE rows (for no-lot direct match)
        all_rows.extend(deduped)
        for row in deduped:
            he_row = dict(row)
            he_row['proj'] = 'HE'
            all_rows.append(he_row)
        
        print("  {} -> CHG {}: {} clean + {} extra low = {} total (HEA+HE)".format(
            sheet_name, chg, len(clean), len(low_extra), len(deduped)))
    
    return all_rows


def main():
    print("=== Convirtiendo FT a CSV (v7 - M107 only, no M483A1) ===\n")
    
    all_csv = []
    
    print("FT155 (M107 HE only, C1 column):")
    wb155 = openpyxl.load_workbook(FT155_PATH, data_only=True)
    ft155 = extract_ft155(wb155)
    all_csv.extend(ft155)
    print("  Subtotal: {} rows\n".format(len(ft155)))
    
    print("FT105 (M1):")
    wb105 = openpyxl.load_workbook(FT105_PATH, data_only=True)
    ft105 = extract_ft105(wb105)
    all_csv.extend(ft105)
    print("  Subtotal: {} rows\n".format(len(ft105)))
    
    # Write CSV
    os.makedirs(os.path.dirname(OUTPUT_PATH) or '.', exist_ok=True)
    
    def sort_key(row):
        num = ''.join(c for c in row['chg'] if c.isdigit())
        return (row['art'], row['proj'], int(num) if num else 0, row['dist'])
    
    all_csv.sort(key=sort_key)
    
    with open(OUTPUT_PATH, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['ART','PROJ','CHG','DIST','QE','TOF','DRIFT','ANGLE','VTERM'])
        for r in all_csv:
            w.writerow([r['art'],r['proj'],r['chg'],r['dist'],
                        r['qe'],r['tof'],r['drift'],r['angle'],r['vterm']])
    
    print("Total: {} rows -> {}".format(len(all_csv), OUTPUT_PATH))
    
    # ===== VERIFICATION =====
    print("\n=== VERIFICACION ===")
    art155 = [r for r in all_csv if r['art'] == 155]
    art105 = [r for r in all_csv if r['art'] == 105]
    
    # 155mm check
    print("\n--- 155mm CHG6W near 10466m ---")
    for r in art155:
        if r['chg']=='6W' and abs(r['dist']-10466)<200:
            print("  D={} Q={} T={} DR={}".format(r['dist'],r['qe'],r['tof'],r['drift']))
    
    # 105mm low-angle monotonicity
    chg105 = sorted(set(r['chg'] for r in art105), key=lambda x: int(x))
    print("\n--- 105mm Low-Angle per charge ---")
    for chg in chg105:
        low = [r for r in art105 if r['chg'] == chg]
        if low:
            mono = all(low[i]['qe'] <= low[i+1]['qe'] for i in range(len(low)-1))
            print("  CHG{}: {} rows, dist {}-{}m, QE {}-{}, mono={}".format(
                chg, len(low), low[0]['dist'], low[-1]['dist'],
                low[0]['qe'], low[-1]['qe'], mono))
            for r in low[-3:]:
                print("    D={:5d} Q={:6.1f} T={:5.1f} DR={:5.1f}".format(
                    r['dist'], r['qe'], r['tof'], r['drift']))
        else:
            print("  CHG{}: no low-angle data".format(chg))


if __name__ == '__main__':
    main()
