import openpyxl

wb = openpyxl.load_workbook(r'C:\Users\DolfoZR\OneDrive\Descargas PC\DESCARGAS_JUL2026\FT105_ADD_R1_M102_Firing_Tables.xlsx', data_only=True)

# Check Document_Notes
ws = wb['Document_Notes']
print("=== Document_Notes ===")
for r in range(1, ws.max_row+1):
    vals = []
    for c in range(1, ws.max_column+1):
        v = ws.cell(r, c).value
        if v is not None:
            vals.append(str(v)[:100])
    if vals:
        print("Row {:3d}: {}".format(r, " | ".join(vals)))

print()
print("=" * 80)
print("=== DEEP ANALYSIS: Separating two trajectories ===")
print("=" * 80)
print()
print("KEY INSIGHT: The FT105 Excel has TWO trajectories interleaved.")
print("After the pure low-angle section, each distance row alternates")
print("between low-angle and high-angle entries.")
print()
print("Strategy: Use TERMINAL VELOCITY as separator")
print("  Low angle:  vterm > 130 (shell is still supersonic/transonic)")
print("  High angle: vterm ~ 80 (shell reaches terminal velocity in steep descent)")
print()

# Analyze Charge 6 in detail (we calibrated with it)
ws6 = wb['Charge_6_TableA']
rows6 = []
for r in range(5, ws6.max_row+1):
    qe = ws6.cell(r, 2).value
    tof = ws6.cell(r, 3).value
    dist = ws6.cell(r, 4).value
    angle = ws6.cell(r, 5).value
    drift = ws6.cell(r, 6).value
    vterm = ws6.cell(r, 7).value
    if qe is not None and dist is not None:
        rows6.append((r, qe, tof, dist, angle, drift, vterm))

# Try to separate using vterm
low_v = [x for x in rows6 if x[6] is not None and isinstance(x[6], (int, float)) and x[6] >= 130]
high_v = [x for x in rows6 if x[6] is not None and isinstance(x[6], (int, float)) and x[6] < 110]
mid_v = [x for x in rows6 if x[6] is not None and isinstance(x[6], (int, float)) and 110 <= x[6] < 130]

print("=== Charge 6 separation by vterm ===")
print("Low angle (vterm >= 130): {} rows".format(len(low_v)))
print("Mid zone  (110 <= vterm < 130): {} rows".format(len(mid_v)))
print("High angle (vterm < 110): {} rows".format(len(high_v)))

# Check if low_v is monotonic in QE
if low_v:
    low_mono = all(low_v[i][1] <= low_v[i+1][1] for i in range(len(low_v)-1))
    print("Low angle QE monotonic: {}".format(low_mono))
    if not low_mono:
        for i in range(len(low_v)-1):
            if low_v[i][1] > low_v[i+1][1]:
                print("  DROP: dist {} QE {} -> dist {} QE {}".format(
                    low_v[i][3], low_v[i][1], low_v[i+1][3], low_v[i+1][1]))
    print("Low angle range: {}m to {}m".format(low_v[0][3], low_v[-1][3]))
    print("Low angle QE: {} to {}".format(low_v[0][1], low_v[-1][1]))

# Check if high_v is monotonic (QE should decrease with distance for high angle)
if high_v:
    high_mono = all(high_v[i][1] >= high_v[i+1][1] for i in range(len(high_v)-1))
    print()
    print("High angle QE monotonic (decreasing): {}".format(high_mono))
    if not high_mono:
        for i in range(len(high_v)-1):
            if high_v[i][1] < high_v[i+1][1]:
                print("  INCREASE: dist {} QE {} -> dist {} QE {}".format(
                    high_v[i][3], high_v[i][1], high_v[i+1][3], high_v[i+1][1]))
    print("High angle range: {}m to {}m".format(high_v[0][3], high_v[-1][3]))
    print("High angle QE: {} to {}".format(high_v[0][1], high_v[-1][1]))

print()
print("=== MID ZONE entries (need manual classification) ===")
for x in mid_v:
    print("Row {:2d}: DIST={:5d} QE={:4d} TOF={:5} ANGLE={:5} DRIFT={} VTERM={}".format(
        x[0], x[3], x[1], x[2], x[4], x[5], x[6]))

# Now check Charge 4 with vterm separation
print()
print("=" * 80)
print("=== Charge 4 separation by vterm ===")
ws4 = wb['Charge_4_TableA']
rows4 = []
for r in range(5, ws4.max_row+1):
    qe = ws4.cell(r, 2).value
    tof = ws4.cell(r, 3).value
    dist = ws4.cell(r, 4).value
    angle = ws4.cell(r, 5).value
    drift = ws4.cell(r, 6).value
    vterm = ws4.cell(r, 7).value
    if qe is not None and dist is not None:
        rows4.append((r, qe, tof, dist, angle, drift, vterm))

low4 = [x for x in rows4 if x[6] is not None and isinstance(x[6], (int, float)) and x[6] >= 130]
high4 = [x for x in rows4 if x[6] is not None and isinstance(x[6], (int, float)) and x[6] < 110]
mid4 = [x for x in rows4 if x[6] is not None and isinstance(x[6], (int, float)) and 110 <= x[6] < 130]

print("Low angle (vterm >= 130): {} rows".format(len(low4)))
print("Mid zone  (110 <= vterm < 130): {} rows".format(len(mid4)))
print("High angle (vterm < 110): {} rows".format(len(high4)))

if low4:
    low4_mono = all(low4[i][1] <= low4[i+1][1] for i in range(len(low4)-1))
    print("Low angle QE monotonic: {}".format(low4_mono))
    if not low4_mono:
        for i in range(len(low4)-1):
            if low4[i][1] > low4[i+1][1]:
                print("  DROP: dist {} QE {} -> dist {} QE {}".format(
                    low4[i][3], low4[i][1], low4[i+1][3], low4[i+1][1]))
    print("Low angle range: {}m to {}m".format(low4[0][3], low4[-1][3]))
    print("Low angle QE: {} to {}".format(low4[0][1], low4[-1][1]))

if high4:
    print()
    high4_mono = all(high4[i][1] >= high4[i+1][1] for i in range(len(high4)-1))
    print("High angle QE monotonic (decreasing): {}".format(high4_mono))
    if not high4_mono:
        for i in range(len(high4)-1):
            if high4[i][1] < high4[i+1][1]:
                print("  INCREASE: dist {} QE {} -> dist {} QE {}".format(
                    high4[i][3], high4[i][1], high4[i+1][3], high4[i+1][1]))
    print("High angle range: {}m to {}m".format(high4[0][3], high4[-1][3]))
    print("High angle QE: {} to {}".format(high4[0][1], high4[-1][1]))

print()
print("=== MID ZONE entries ===")
for x in mid4:
    print("Row {:2d}: DIST={:5d} QE={:4d} TOF={:5} ANGLE={:5} DRIFT={} VTERM={}".format(
        x[0], x[3], x[1], x[2], x[4], x[5], x[6]))
