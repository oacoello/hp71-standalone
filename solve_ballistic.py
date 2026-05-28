import sys
import subprocess
import json
import os

# argumentos desde C++
proj = sys.argv[1]
dist_target = float(sys.argv[2])

BASE_DIR = os.path.dirname(__file__)
BALLISTIC_DIR = os.path.join(BASE_DIR, "ballistic_lab", "ballistic_lab")

# mapear PROJ → archivo JSON
proj_files = {
    "HE": "155mm_HE_M106.json",
    "HEA": "155mm_HE_RAAM_LM718A1.json",
    "ILA": "155mm_ILLUM_M485.json",
    "HERA": "155mm_HERA_M549A1.json",
    "M67": "105mm_SMOKE_HC.json"
}

if proj not in proj_files:
    print("ERROR")
    sys.exit(1)

projectile = f"data/projectiles/{proj_files[proj]}"

# leer cargas disponibles
full_path = os.path.join(BALLISTIC_DIR, projectile)

with open(full_path) as f:
    data = json.load(f)

charges = list(data["charges"].keys())

best_solution = None
best_error = 1e9

# 🔥 buscar solución
for chg in charges:
    for qe in range(300, 700, 5):

        cmd = [
            "python",
            "run_lab.py",
            projectile,
            str(qe),
            "--charge",
            chg
        ]

        result = subprocess.run(
            cmd,
            cwd=BALLISTIC_DIR,
            capture_output=True,
            text=True
        )

        output = result.stdout.strip()

        try:
            lines = output.splitlines()
            last = lines[-1].split()

            dist = float(last[1])
            tof = float(last[2])
            drift = float(last[3])

            error = abs(dist - dist_target)

            if error < best_error:
                best_error = error
                best_solution = (chg, qe, tof, drift)

        except:
            continue

# 🔥 salida para C++
if best_solution:
    chg, qe, tof, drift = best_solution
    print(f"{chg},{qe},{tof},{drift}")
else:
    print("ERROR")