# HP-71B Fire Direction Computer Emulator — Complete Codebase Index

**Branch:** TABLAS_REALES  
**Date:** 2026-08-13  
**Total files:** 62 (source, config, data, backups, web UI)

---

## 1. PROJECT ARCHITECTURE OVERVIEW

```
hp71-standalone/
├── hp71_server/main.cpp          ← HTTP server (httplib) + console, entry point
├── saturn_core/
│   ├── basic_engine.cpp/.h       ← CORE: All menu logic, fire mission flow, ballistic solve (5126 lines)
│   ├── ballistic_engine.cpp/.h   ← Legacy/simple ballistic interpolation (NOT main solver)
│   ├── ballistic_tables.cpp/.h   ← Legacy hardcoded tables (105/155), NOT used for fire missions
│   ├── map_model.cpp/.h          ← Standalone MapModel class (NOT integrated into basic_engine)
│   ├── hp71_lcd.cpp/.h           ← LCD display emulation
│   ├── hardware_map.cpp/.h       ← Hardware memory mapping (stub)
│   ├── keyboard_matrix.cpp/.h    ← Keyboard matrix emulation
│   ├── saturn_cpu.cpp/.h         ← Saturn CPU emulator (NOT used by FDC)
│   ├── saturn_alu.cpp/.h         ← Saturn ALU operations
│   ├── saturn_memory.cpp/.h      ← Saturn memory (ROM/RAM)
│   ├── saturn_decoder.cpp/.h     ← Saturn instruction decoder
│   ├── saturn_field.cpp/.h       ← Saturn field operations
│   ├── saturn_shift.cpp/.h       ← Saturn shift/rotate operations
│   ├── saturn_flags.cpp/.h       ← Saturn flag operations
│   ├── saturn_timer.cpp/.h       ← Saturn timer
│   └── saturn_compare.cpp/.h     ← Saturn comparison operations
├── web_ui/
│   ├── index.html                ← Main web UI (calculator emulator with multi-piece support)
│   ├── hp71b.html                ← Alternate HP-71B BASIC emulator UI (unused by FDC)
│   ├── keyboard.js               ← Keyboard event handler (for hp71b.html)
│   └── lcd.html                  ← LCD display component
├── auth/
│   ├── user_auth.cpp/.h          ← Auth module (DISABLED - always returns success)
│   └── users.json                ← User credentials (unused)
├── tables/
│   ├── tables.csv                ← PRIMARY firing tables (ART,PROJ,CHG,DIST,QE,TOF,DRIFT,ANGLE,VTERM)
│   ├── tables_backup_*.csv       ← Backup tables
│   └── README.md
├── external/httplib/httplib.h    ← cpp-httplib single-header HTTP library
├── CMakeLists.txt                ← Top-level CMake
├── hp71_server/CMakeLists.txt    ← Server build config
├── VARIABLES_BALISTICAS.txt      ← Ballistic variable documentation (394 lines)
├── bateria_bravo_comandos.txt    ← Real fire mission test data
├── electron_payload/             ← Electron app packaging (backend + index.html variants)
├── .sdd/                         ← SDD exploration files
└── .engram/                      ← Persistent memory database
```

---

## 2. MENU SYSTEM — COMPLETE CATALOG (71 menu states)

### 2.1 MENU MAIN (lines 1710-1757)
- **String:** `"MAIN (? 1 3 4 5 7 X *)"`
- **Options:**
  - `1` → MENU_FM (Fire Mission)
  - `3` → MENU_TARGET (set target)
  - `4` → MENU_OBS (set observer)
  - `5` → MENU_AFU (auxiliary functions)
  - `7` → MENU_MAP_MODEL (map model)
  - `X` → resetData()
  - `*` → re-render main menu
- **P handler:** No
- **Enter-empty:** Shows main menu again

### 2.2 MENU_FM (lines 2506-2867)
- **String:** `"FM (? 1 2 3 4 R E P X *)"`
- **Options:**
  - `1` → MENU_FM1_TGT (Area Fire / FM1)
  - `2` → MENU_REG (Registration / FM2)
  - `3` → MENU_SHIFT_PREV_DIR (Doctrinal shift / FM3)
  - `4` → MENU_FM4_LR (PMI / FM4)
  - `S` → MENU_SHEAF (sheaf type)
  - `R` → Review all data (COB, TGT, AMMO, OBS, MAP)
  - `A` → All guns FFE mode
  - `T` → MENU_SHIFT_PREV_DIR (same as 3)
  - `X` → Execute fire mission (renderFire)
  - `E` → End of Mission
  - `P` → back to MENU_FM
  - `*` → MENU_MAIN
  - `AUTOCHG` → disable manual charge
  - `CD0=...` → set STANAG cd0
  - `V0_XX=...` → set muzzle velocity per charge
  - `TEMP=...` → set temperature
  - `HUM=...` → set humidity
  - `WIND_DIR=...` → set wind direction
  - `WIND_SPD=...` → set wind speed
  - `FIRING_AZ=...` → set firing azimuth
  - `SHOW` → display STANAG config
  - `STANAG` → compare HP-71B vs STANAG
  - `STANAG_CAL` → calibrate cd0
- **P handler:** Shows menu
- **Enter-empty:** Shows menu
- **Connects to:** renderFire via X

### 2.3 MENU_FM1_TGT (lines 2869-2908)
- **String:** `"TGT (? 1 2 3 P *)"`
- **Options:**
  - `1` → MENU_FM1_GRID (Grid coordinates)
  - `2` → MENU_FM1_TRANSPORT (Transport from known point)
  - `3` → MENU_FM1_POLAR (Polar coordinates)
  - `P`/`*` → MENU_FM

### 2.4 MENU_FM1_GRID (lines 2910-3019)
- **Input stages (4):**
  - Stage 0: TGT/KNPT # (knpt identifier)
  - Stage 1: E (easting)
  - Stage 2: N (northing)
  - Stage 3: ALT (altitude)
- **P handler:** Steps back through stages (shows current value)
- **Enter-empty:** Maintains previous value
- **Connects to:** finishTgtBaseShot → renderFire

### 2.5 MENU_FM1_TRANSPORT (lines 3021-3181)
- **Input stages (8):**
  - Stage 0: DESDE (from KNPT #)
  - Stage 1: AZ (azimuth in mils)
  - Stage 2: I/D (Izquierda/Derecha = Left/Right)
  - Stage 3: VAL I/D (distance value)
  - Stage 4: +/- (Add/Drop = forward/backward)
  - Stage 5: VAL +/- (distance value)
  - Stage 6: S/B (Sube/Baja = Up/Down)
  - Stage 7: VAL S/B (altitude value)
- **P handler:** Steps back
- **Enter-empty:** Maintains value
- **Connects to:** finishTgtBaseShot → renderFire

### 2.6 MENU_FM1_POLAR (lines 3183-3294)
- **Input stages (4):**
  - Stage 0: AZ (azimuth)
  - Stage 1: DIST (distance)
  - Stage 2: S/B (Sube/Baja)
  - Stage 3: VAL S/B (altitude delta)
- **P handler:** Steps back
- **Enter-empty:** Maintains value
- **Connects to:** finishTgtBaseShot → renderFire

### 2.7 MENU_FM1_BASE_PIECE (lines 4179-4254)
- **String:** `"BASE PIECE (P *): N"`
- **Input:** Base piece number
- **P handler:** MENU_FM
- **Enter-empty:** Uses current base_piece_index
- **Connects to:** renderFire, logs ShotLog, goes to MENU_COMP_CORR

### 2.8 MENU_FM4_LR (lines 3296-3320)
- **String:** `"IMPACT L/R (ej: L50 o R50):"`
- **Input:** L/R + value (e.g., L50, R30)
- **P handler:** No (X returns to MENU_FM)
- **Connects to:** MENU_FM4_AD

### 2.9 MENU_FM4_AD (lines 3322-3339)
- **String:** `"IMPACT A/D (A o D):"`
- **Input:** A/D + value
- **Connects to:** MENU_FM4_UD

### 2.10 MENU_FM4_UD (lines 3342-3359)
- **String:** `"IMPACT U/D (U o D):"`
- **Input:** U/D + value
- **Connects to:** MENU_FM4_NEXT

### 2.11 MENU_FM4_NEXT (lines 3362-3405)
- **String:** `"ADD MORE? (Y/N)"`
- **Y** → back to MENU_FM4_LR
- **N** → average all inputs, applyObserverCorrection, renderFire

### 2.12 MENU_TARGET (lines 1759-1852)
- **Input stages (5):**
  - Stage 0: TGT indicator (name/label)
  - Stage 1: KNPT # (known point number)
  - Stage 2: TGT E (easting)
  - Stage 3: TGT N (northing)
  - Stage 4: TGT ALT (altitude)
- **P handler:** Steps back (shows current value with "(P *)")
- **Enter-empty:** Maintains previous value
- **Connects to:** Stores in `targets` map, returns to MENU_MAIN

### 2.13 MENU_OBS (lines 2206-2357)
- **Input stages (4):**
  - Stage 0: PO # (observer ID)
  - Stage 1: E (easting)
  - Stage 2: N (northing)
  - Stage 3: ALT (altitude)
- **P handler:** Steps back
- **Enter-empty:** Maintains previous value
- **Connects to:** Stores in `observers` map, returns to MENU_MAIN

### 2.14 MENU_AFU (lines 1854-1877)
- **String:** `"AFU INDEX (? 1 3 5 *)"`
- **Options:**
  - `1` → MENU_ART_TYPE (artillery type)
  - `3` → MENU_MET (meteorology)
  - `5` → MENU_AMMO (ammunition)
  - `*` → MENU_MAIN

### 2.15 MENU_ART_TYPE (lines 1879-1889)
- **String:** `"ART (105/155):"`
- **Input:** 105 or 155
- **Connects to:** MENU_COB_QTY

### 2.16 MENU_AMMO (lines 1891-1950)
- **String:** `"AMMO FILE (? I *)"`
- **I** → input mode (PROJ, LOT)
- **P handler:** Steps back
- **Enter-empty:** Maintains value

### 2.17 MENU_MET (lines 2468-2504)
- **Input stages (3):**
  - Stage 0: DIR (wind direction)
  - Stage 1: VEL (wind speed)
  - Stage 2: TEMP (temperature)
- **P handler:** Steps back
- **Enter-empty:** Maintains value

### 2.18 MENU_MAP_MODEL (lines 2359-2466)
- **Input stages (6):**
  - Stage 0: MAX E
  - Stage 1: MIN E
  - Stage 2: MAX N
  - Stage 3: MIN N
  - Stage 4: GZ (grid zero)
  - Stage 5: SPHER (spheroid)
- **P handler:** Steps back
- **Enter-empty:** Maintains value
- **Special:** Auto-detects weather zone (Zambrano, Pinalejo, Trincheras) based on map center

### 2.19 MENU_COB_QTY (lines 1952-1967)
- **String:** `"QTY PIECE:"`
- **P handler:** Shows current value
- **Enter-empty:** Maintains value

### 2.20 MENU_COB_BASE (lines 1970-1995)
- **String:** `"BASE PIECE (P *): N"`
- **P handler:** Returns to MENU_COB_QTY
- **Enter-empty:** Maintains value

### 2.21 MENU_COB_GB_E (lines 1996-2011)
- **String:** `"GB E (P *): value"`
- **P handler:** Returns to MENU_COB_BASE
- **Enter-empty:** Maintains value

### 2.22 MENU_COB_GB_N (lines 2013-2028)
- **String:** `"GB N (P *): value"`
- **P handler:** Returns to MENU_COB_GB_E
- **Enter-empty:** Maintains value

### 2.23 MENU_COB_GB_ALT (lines 2030-2053)
- **String:** `"GB ALT (P *): value"`
- **P handler:** Returns to MENU_COB_GB_N
- **Enter-empty:** Maintains value

### 2.24 MENU_COB_AZ_LAY (lines 2055-2070)
- **String:** `"AZ LAY (P *): value"`
- **P handler:** Returns to MENU_COB_GB_ALT
- **Enter-empty:** Maintains value

### 2.25 MENU_COB_REF_DEF (lines 2072-2106)
- **String:** `"REF DEF (P *): value"`
- **P handler:** Returns to MENU_COB_AZ_LAY
- **Enter-empty:** Maintains value
- **Special:** Saves COB data to main_inputs, starts gun entry loop

### 2.26 MENU_COB_DIR (lines 2108-2126)
- **String:** `"#N DIR (P *): value"`
- **Input:** Direction for gun N (polar from GB)
- **P handler:** Returns to MENU_COB_REF_DEF

### 2.27 MENU_COB_DIST (lines 2128-2149)
- **String:** `"#N DIST (P *): value"`
- **Input:** Distance for gun N
- **P handler:** Returns to MENU_COB_DIR

### 2.28 MENU_COB_IV (lines 2151-2204)
- **String:** `"#N IV (P *): value"`
- **Input:** Incremento Vertical for gun N
- **P handler:** Returns to MENU_COB_DIST
- **Special:** Calculates gun position (E, N, ALT) from GB + polar + IV, stores in `guns` vector

### 2.29 MENU_REG (lines 3793-4121)
- **String:** `"KNPT #:"` or `"USE LAST REG DATA? (Y/N)"`
- **Input stages (8):**
  - Stage -1: Use last data? (Y/N)
  - Stage 0: KNPT #
  - Stage 1: MET ENG
  - Stage 2: MET CNTL
  - Stage 3: PROJ (projectile)
  - Stage 4: PROJ LOT
  - Stage 5: FUZE (TIA/PDA)
  - Stage 6: REG RG (registration range)
  - Stage 7: REG DEF (registration deflection)
- **P handler:** Steps back
- **Enter-empty:** Maintains value (uses last solution as default)
- **Connects to:** renderFire → MENU_COMP_CORR

### 2.30 MENU_REG_BASE_PIECE (lines 4123-4177)
- **String:** `"BASE PIECE (P *): N"`
- **P handler:** Returns to MENU_REG (FUZE stage)
- **Enter-empty:** Maintains value
- **Connects to:** renderFire → MENU_REG

### 2.31 MENU_COMP_CORR (lines 4256-4309)
- **String:** `"COMP CORR (Y N P *)"`
- **Options:**
  - `Y` → MENU_TIME_REG (yes, apply corrections)
  - `N` → MENU_INST_PREV_DIR (no, instant correction)
  - `P` → MENU_FM
  - `X`/empty + chg_allowed → MENU_CHG_EDIT (edit charge)
- **Special:** Gateway between fire and corrections

### 2.32 MENU_CHG_EDIT (lines 4877-4904)
- **String:** `"Cg:"` (charge edit)
- **Input:** New charge value
- **Sets:** manual_chg_enabled = true, manual_chg_value = cmd
- **Returns to:** MENU_FM

### 2.33 MENU_TIME_REG (lines 4711-4734)
- **String:** `"TIME REG (Y N P *)"`
- **Y** → MENU_TIME_REG_FUZE
- **P** → MENU_TIME_REG_INPUT
- **N** → MENU_FM

### 2.34 MENU_TIME_REG_INPUT (lines 4736-4741)
- **String:** `"TIME CORR:"`
- **Input:** Time correction value
- **Returns to:** MENU_FM

### 2.35 MENU_TIME_REG_FUZE (lines 4743-4768)
- **String:** `"FUZE (P*) TIA/PDA"`
- **PDA** → fuze_time_mode=false, MENU_UD_CORR
- **TIA** → fuze_time_mode=true, MENU_TIME_REG_HOB

### 2.36 MENU_TIME_REG_HOB (lines 4770-4780)
- **String:** `"TOTAL HOB (*):"`
- **Input:** Height of burst value
- **Returns to:** MENU_UD_CORR

### 2.37 MENU_UD_CORR (lines 4782-4827)
- **String:** `"(U/D) CORR (*)"`
- **Input:** U/D correction (e.g., U10, D5)
- **X** → skip, return to MENU_FM
- **Special:** Applies UD correction, renders fire, returns to MENU_COMP_CORR

### 2.38 MENU_DF_CORR (lines 4829-4875)
- **String:** `"DF CORR (ADD/DROP LEFT/RIGHT *)"`
- **Input:** Direction + value
- **ADD/DROP** → modifies reg_dist
- **LEFT/RIGHT** → modifies df_corr

### 2.39 MENU_SHIFT_PREV_DIR (lines 3407-3412)
- **String:** `"PREV DIR:"`
- **Input:** Previous direction value

### 2.40 MENU_SHIFT_PREV_LR (lines 3414-3449)
- **String:** `"PREV L/R:"`
- **Input:** L/R + value

### 2.41 MENU_SHIFT_PREV_AD (lines 3451-3486)
- **String:** `"PREV A/D:"`
- **Input:** A/D + value

### 2.42 MENU_SHIFT_PREV_UD (lines 3488-3523)
- **String:** `"PREV U/D:"`
- **Input:** U/D + value

### 2.43 MENU_SHIFT_DIR (lines 3525-3559)
- **String:** `"DIR:"`
- **Input:** New direction
- **Special:** Calculates ANGLE T automatically

### 2.44 MENU_SHIFT_ANGLE (lines 3561-3570)
- **String:** `"ANGLE T: value"`
- **Input:** Angle T (can be overridden)

### 2.45 MENU_SHIFT_LR (lines 3572-3607)
- **String:** `"L/R SHIFT:"`
- **Input:** L/R shift value

### 2.46 MENU_SHIFT_AD (lines 3609-3644)
- **String:** `"A/D SHIFT:"`
- **Input:** A/D shift value

### 2.47 MENU_SHIFT_UD (lines 3646-3702)
- **String:** `"U/D SHIFT:"`
- **Input:** U/D shift value
- **Special:** Applies all corrections via applyObserverCorrection, sets fire_phase=3

### 2.48 MENU_SHIFT (lines 3704-3752)
- **String:** `"SHIFT (L/R A/D U/D):"`
- **Input:** Single correction command
- **Special:** Direct apply via applyObserverCorrection

### 2.49 MENU_SHEAF (lines 3754-3771)
- **String:** `"SHEAF (CONV/OPEN):"`
- **CONV** → sheaf_mode="CONV", MENU_FM
- **OPEN** → sheaf_mode="OPEN", MENU_SHEAF_WIDTH

### 2.50 MENU_SHEAF_WIDTH (lines 3773-3791)
- **String:** `"OPEN WIDTH (MILS):"`
- **Input:** Width in mils
- **Returns to:** MENU_FM

### 2.51 MENU_INST_PREV_DIR (lines 4311-4326)
- **String:** `"PREV DIR (*):"`
- **Input:** Previous direction
- **P handler:** Returns to MENU_COMP_CORR

### 2.52 MENU_INST_PREV_LR (lines 4328-4371)
- **String:** `"PREV L/R (P*):"`
- **P handler:** Returns to MENU_INST_PREV_DIR

### 2.53 MENU_INST_PREV_AD (lines 4373-4416)
- **String:** `"PREV A/D (P*):"`
- **P handler:** Returns to MENU_INST_PREV_LR

### 2.54 MENU_INST_PREV_UD (lines 4418-4467)
- **String:** `"PREV U/D (P*):"`
- **P handler:** Returns to MENU_INST_PREV_AD

### 2.55 MENU_INST_DIR (lines 4469-4511)
- **String:** `"DIR (P X): value"`
- **Input:** New direction
- **Special:** Calculates ANGLE T automatically

### 2.56 MENU_INST_ANGLE_T (lines 4513-4535)
- **String:** `"ANG T (P*): value"`
- **Input:** Angle T (can override)

### 2.57 MENU_INST_LR_SHIFT (lines 4537-4586)
- **String:** `"(L/R) SHIFT (P*):"`
- **Input:** L/R shift

### 2.58 MENU_INST_AD_SHIFT (lines 4588-4631)
- **String:** `"(A/D) SHIFT (P*):"`
- **Input:** A/D shift

### 2.59 MENU_INST_UD_SHIFT (lines 4633-4709)
- **String:** `"(U/D) SHIFT (P*):"`
- **Input:** U/D shift
- **Special:** Final stage — applies all corrections, renders fire, logs ShotLog, returns to MENU_COMP_CORR

---

## 3. BALLISTIC SOLVING SYSTEM

### 3.1 Primary Solver (basic_engine.cpp, static functions)

#### `interp()` (line 383)
- Quadratic interpolation on firing table rows
- Returns: qe, tof, drift
- Fallback: linear interpolation if quadratic fails

#### `solveAuto()` (line 426)
- Searches all firing tables matching artillery_type + projectile + lot
- Finds best charge for given distance
- Validates: qe > 0, tof > 0, qe < 1200, tof < 120, TOF within expected range
- Returns: charge, qe, tof, drift

#### `solveByCharge()` (line 517)
- Searches firing tables for specific charge + projectile + lot
- Used when manual_chg_enabled is true

#### `solve()` (line 573)
- Dispatcher: if manual_chg_enabled → solveByCharge, else → solveAuto

#### `hp71bCalibrate()` (line 625)
- **HP-71B empirical formula** that overrides STANAG results
- 155mm only (returns immediately for 105mm)
- Parameters per charge: curve, power, qe_bias, tof_scale, tof_bias
- HEA projectile + charge 6 special low-curve factor (9000-10000m)
- Formula: `qe = ((base * km) + (curve * km²) + (chg_num * chg_scale)) / power + qe_bias`

### 3.2 STANAG 4355 Physics Model (Stanag4355 namespace, lines 703-1012)

#### `g1_cd()` (line 718)
- G1 drag coefficient table interpolation (25 Mach points)

#### `atmosphere()` (line 748)
- ISA atmosphere model (temperature, pressure, density, speed of sound)
- Humidity correction

#### `rk4_step()` (line 885)
- 4th-order Runge-Kutta integration step

#### `compute_trajectory()` (line 919)
- Full trajectory simulation with dt=0.005s
- Max 200,000 steps, max 200s or 25km

#### `solve_qe()` (line 961)
- Binary search for quadrant elevation
- Phase 1: scan 1-59 degrees for max range
- Phase 2: 25-iteration binary search

#### `charge_to_v0()` (line 800)
- Maps charge names to muzzle velocities
- Supports: 3G, 4G, 5G, 3W, 4W, 5W, 6W, 7W, 7R, 8S

### 3.3 Comparison/Calibration Functions

#### `stanagCompare()` (line 1014)
- Compares HP-71B empirical vs STANAG 4355 results
- Includes spin drift and Coriolis calculations

#### `stanagCalibrate()` (line 1075)
- Automated cd0 optimization against firing tables
- Tests 15 cd0 values (0.130-0.200)
- Reports RMSE, MAXERR, split G/W optimization

### 3.4 Legacy BallisticEngine (ballistic_engine.cpp)
- Simple linear interpolation on 3 hardcoded tables (105 C5, 105 C6, 155 M4)
- **NOT used by fire mission system** — basic_engine.cpp uses its own solver

### 3.5 Legacy BallisticTables (ballistic_tables.cpp)
- Hardcoded tables for 105mm and 155mm (10-16 entries each)
- **NOT used by fire mission system**

---

## 4. FIRE MISSION RENDERING

### `renderFire()` lambda (lines 1315-1551)
- Core rendering function for all fire missions
- Calculates per-piece: distance, azimuth, deflection, QE, TOF, drift
- Applies:
  - Registration corrections (reg_dist, reg_def)
  - Direction of fire correction (df_corr)
  - Time registration correction
  - UD correction
  - Drift (from table or default formula)
  - Jump horizontal (6.6 mils, zeroed in FM1 reverse mode)
  - Sheaf (CONV or OPEN with width)
  - Shift corrections (shift_lr, shift_ad via shift_angle rotation)
- Outputs per piece: DIST, AZ, DEF, CHG, QE, TOF, FUZE

---

## 5. ALL STATIC/GLOBAL VARIABLES

### State Machine
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `boot_mode` | bool | true | Boot sequence active |
| `initializing` | bool | false | Post-boot initialization |
| `current_menu` | Menu | MENU_MAIN | Active menu state |
| `input_stage` | int | 0 | Current input stage within menu |
| `fire_phase` | int | 0 | Fire mission phase (0-4) |

### Gun Positions
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `gb_e`, `gb_n`, `gb_alt` | double | 0.0 | Gun base position |
| `az_lay` | double | 0.0 | Azimuth of lay |
| `def_base` | int | 3200 | Reference deflection |
| `base_piece_index` | int | 0 | Index of base piece |
| `cob_expected_qty` | int | 0 | Expected number of guns |
| `cob_current_index` | int | 0 | Current gun being entered |
| `guns` | vector\<Gun\> | — | All gun positions |
| `temp_dir`, `temp_dist`, `temp_iv` | double | 0.0 | Temp storage during COB entry |

### Target
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `tgt_e`, `tgt_n`, `tgt_alt` | double | 0.0 | Current target position |
| `current_knpt` | int | 0 | Current known point number |
| `current_tgt_indicator` | string | "" | Target label |
| `targets` | map\<int,TargetData\> | — | All stored targets |

### Observer
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `obs_e`, `obs_n`, `obs_alt` | double | 0.0 | Observer position |
| `obs_current_index` | int | 1 | Next observer ID |
| `observers` | map\<int,ObserverData\> | — | All stored observers |
| `BasicEngine::obs_id` | int (static) | 0 | Current observer ID |
| `BasicEngine::obs_gz` | double (static) | 0 | Grid zone for observer |

### Meteorology
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `wind_dir` | double | 0.0 | Wind direction |
| `wind_speed` | double | 2.5 | Wind speed (Honduras avg) |
| `temperature` | double | 28.0 | Temperature (Honduras avg) |

### Map Model
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `map_e_max`, `map_e_min` | double | 0.0 | Map East bounds |
| `map_n_max`, `map_n_min` | double | 0.0 | Map North bounds |
| `map_gz` | double | 0.0 | Grid zone |
| `map_spher` | string | "" | Spheroid name |

### Ammunition
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `ammo_proj_prop` | string | "" | Projectile type (HEA, HE) |
| `ammo_proj_lot` | string | "" | Projectile lot |
| `ammo_proj_wt` | double | 0 | Projectile weight |
| `artillery_type` | string | "155" | Artillery caliber |
| `mission_type` | string | "" | Mission type |
| `manual_chg_enabled` | bool | false | Manual charge override active |
| `manual_chg_value` | string | "" | Manual charge value |

### Fuze / Corrections
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `fuze_time_mode` | bool | false | TIA (time) vs PDA (proximity) |
| `hob` | double | 0 | Height of burst |
| `reg_dist` | double | 0.0 | Registration distance correction |
| `reg_def` | double | 0.0 | Registration deflection correction |
| `df_corr` | double | 0.0 | Direction of fire correction |
| `time_reg_correction` | double | 0.0 | Time registration correction |
| `ud_corr` | double | 0.0 | Up/down correction |
| `ud_active` | bool | false | UD correction active |

### Shift Corrections
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `shift_lr` | double | 0.0 | Accumulated L/R shift |
| `shift_ad` | double | 0.0 | Accumulated A/D shift |
| `shift_ud` | double | 0.0 | Accumulated U/D shift |
| `shift_angle` | double | 0.0 | Shift angle (mils) |
| `shift_prev_dir` | string | "N" | Previous direction for shift |
| `shift_prev_lr`, `shift_prev_ad`, `shift_prev_ud` | double | 0.0 | Previous shift values |
| `shift_new_dir` | string | "N" | New direction for shift |
| `last_dist_solution` | double | 0.0 | Last distance solution |
| `last_def_solution` | double | 0.0 | Last deflection solution |
| `last_qe_solution` | double | 0.0 | Last QE solution |

### Instantaneous Correction (FM3)
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `inst_prev_dir`, `inst_prev_lr`, `inst_prev_ad`, `inst_prev_ud` | double | 0.0 | Previous shot data |
| `inst_new_dir`, `inst_angle_t`, `inst_last_dir` | double | 0.0 | New direction data |
| `inst_lr_shift`, `inst_ad_shift`, `inst_ud_shift` | double | 0.0 | Instant corrections |

### FM1 Transport
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `fm1_tgt_method` | int | 0 | Target method (1=grid, 2=transport, 3=polar) |
| `fm1_temp_knpt` | int | 0 | Temp KNPT during input |
| `fm1_temp_e`, `fm1_temp_n`, `fm1_temp_alt` | double | 0.0 | Temp target during input |
| `fm1_base_def_reverse` | bool | false | Base deflection reverse mode |
| `active_fm1_def_reverse` | bool | false | Active FM1 reverse |
| `active_fm1_transport_qe_shape` | bool | false | Transport QE shape active |
| `fm1_grid_clean_fire_pending` | bool | false | Grid clean fire pending |
| `fm1_grid_*_backup` | double/bool | 0/false | Backup values for grid fire |
| `fm1_pol_az`, `fm1_pol_dist` | double | 0.0 | Polar azimuth/distance |
| `fm1_pol_ud`, `fm1_pol_ud_val` | char/double | 'N'/0.0 | Polar up/down |
| `fm1_from_knpt` | int | 0 | Transport source KNPT |
| `fm1_tr_az`, `fm1_tr_lr`, `fm1_tr_ad`, `fm1_tr_ud` | double/char | varies | Transport parameters |

### Sheaf
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `sheaf_mode` | string | "CONV" | CONV or OPEN |
| `sheaf_width` | double | 60.0 | Open sheaf width (mils) |

### Mission State
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `mission_active` | bool | false | Mission in progress |
| `mission_counter` | int | 1 | Mission number |
| `ffe_mode` | bool | false | Free fire area mode |
| `all_guns_command` | bool | false | All guns firing |
| `reg_data_available` | bool | false | Registration data available |
| `last_solution` | string | "" | Last fire solution text |
| `chg_allowed` | bool | false | Charge edit allowed |
| `chg_edit_mode` | bool | false | In charge edit mode |
| `chg_wait_value` | bool | false | Waiting for charge value |
| `ammo_input_active` | bool | false | Ammo input mode active |

### Mission Log
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `mission_log` | vector\<ShotLog\> | — | All shots fired this mission |
| `main_inputs` | vector\<string\> | — | Input log for review |
| `last_inputs` | vector\<string\> | — | Inputs for current shot |
| `last_knpt`, `last_proj`, `last_lot` | int/string | varies | Last registration data |
| `last_fuze_tia` | bool | false | Last fuze mode |
| `last_reg_rg`, `last_reg_def` | double | 0.0 | Last registration values |

### STANAG Configuration (namespace scope)
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `Stanag4355::cfg_cd0` | double | 0.157 | Drag coefficient |
| `Stanag4355::cfg_temp` | double | 28.0 | Temperature |
| `Stanag4355::cfg_humidity` | double | 75.0 | Humidity |
| `Stanag4355::cfg_wind_dir` | double | 0.0 | Wind direction |
| `Stanag4355::cfg_wind_spd` | double | 2.5 | Wind speed |
| `Stanag4355::cfg_firing_az` | double | 0.0 | Firing azimuth |
| `Stanag4355::cfg_v0` | map\<string,double\> | — | Muzzle velocity overrides |

### Firing Tables (static)
| Variable | Type | Purpose |
|----------|------|---------|
| `firingTables` | map\<AmmoKey, vector\<Row\>\> | All loaded firing tables from CSV |

---

## 6. LAMBDAS AND HELPER FUNCTIONS (inside execute())

| Lambda/Function | Lines | Purpose |
|-----------------|-------|---------|
| `renderFire` | 1315-1551 | Main fire solution renderer (per-piece calculation) |
| `computeAngleTFromDir` | 1553-1579 | Computes angle T from direction in mils |
| `computeBaseAz` | 1581-1608 | Computes base piece azimuth to target |
| `applyObserverCorrection` | 1610-1633 | Applies observer corrections (rotated by angle T) |
| `prepareTgtBaseShotForObserverCorrections` | 1635-1664 | Resets all corrections for clean fire |
| `finishTgtBaseShot` | 1666-1673 | Completes FM1 target entry, goes to BASE PIECE |
| `normStr()` | 29-49 | Trims whitespace from strings |
| `parseUD()` | 1234-1270 | Parses U/D correction strings |
| `getCurrentTime()` | 1272-1281 | Returns formatted timestamp |
| `computeSite()` | 581-585 | Computes site angle from IV and distance |
| `extractChargeNumber()` | 587-598 | Extracts numeric charge from string |
| `smoothstepCal()` | 600-604 | Smooth interpolation function |
| `chg6LowCurveFactor()` | 606-623 | Charge 6 low-curve correction factor |

---

## 7. MENU FLOW DIAGRAMS

### 7.1 Main Fire Mission Flow
```
MAIN (1) → FM
  FM (1) → FM1_TGT → FM1_GRID/TRANSPORT/POLAR → finishTgtBaseShot → FM1_BASE_PIECE → renderFire → COMP_CORR
  FM (2) → REG → [KNPT → MET → PROJ → LOT → FUZE → BASE_PIECE → REG_RG → REG_DEF] → renderFire → COMP_CORR
  FM (3) → SHIFT flow → [PREV_DIR → PREV_LR → PREV_AD → PREV_UD → DIR → ANGLE → LR → AD → UD] → renderFire
  FM (4) → FM4 flow → [LR → AD → UD → NEXT] → average → applyObserverCorrection → renderFire
  FM (X) → renderFire → COMP_CORR
  COMP_CORR (Y) → TIME_REG → [FUZE → HOB] → UD_CORR → renderFire → COMP_CORR
  COMP_CORR (N) → INST flow → [PREV_DIR → PREV_LR → PREV_AD → PREV_UD → DIR → ANGLE_T → LR → AD → UD] → renderFire → COMP_CORR
  COMP_CORR (X) → CHG_EDIT → [charge value] → FM
```

### 7.2 Data Entry Flow
```
MAIN (3) → TARGET → [indicator → KNPT → E → N → ALT] → MAIN
MAIN (4) → OBS → [PO# → E → N → ALT] → MAIN
MAIN (5) → AFU → (1) ART_TYPE → COB flow / (3) MET → [DIR → VEL → TEMP] / (5) AMMO → [PROJ → LOT]
MAIN (7) → MAP_MODEL → [MAX_E → MIN_E → MAX_N → MIN_N → GZ → SPHER] → MAIN
```

---

## 8. WEBSOCKET/HTTP INTERFACE

### Server (hp71_server/main.cpp)
- **POST /input** — Receives `cmd` parameter, calls `engine.execute(cmd)`, returns text
- **GET /** — Serves index.html from multiple search paths
- **Port:** 127.0.0.1:8080

### Web UI (web_ui/index.html)
- LCD display with green-on-black terminal aesthetic
- Virtual calculator keyboard (QWERTY + numeric pad)
- **Multi-piece navigation:** UP/DOWN arrows cycle through fire solution pieces
- **Base piece tracking:** Detects BASE PIECE prompts, tracks which piece is base
- **U/D mode:** Activates when system requests U/D correction
- **Boot sequence:** RUN → "RUNBUCS" → INITIALIZE → auto-press ENTER after 2s
- Physical keyboard support (all keys mapped)

---

## 9. TODOs, INCOMPLETE FEATURES, AND PLACEHOLDERS

### 9.1 Incomplete/Stub Code
1. **`hardware_map.cpp`** (line 1) — Only includes header, no implementation
2. **`map_model.cpp`** — Standalone class NOT integrated into basic_engine.cpp (basic_engine has its own MAP_MODEL handler)
3. **`ballistic_engine.cpp`** — Legacy solver with only 3 hardcoded tables, NOT used by fire missions
4. **`ballistic_tables.cpp`** — Legacy tables, NOT used by fire missions
5. **`auth/user_auth.cpp`** (line 10-11) — Auth disabled: `"LOGIN DESACTIVADO PARA DESARROLLO"`
6. **Saturn CPU emulator** (saturn_*.cpp) — Full HP Saturn CPU emulation exists but is NOT used by the FDC system

### 9.2 Potential Issues / Notes
1. **Missing break statements:** Several case blocks in the switch (e.g., MENU_AMMO at line 1950, MENU_COB_QTY at line 1967) fall through to the next case — appears intentional for flow but risky
2. **Static variables in switch:** `fm4_lr`, `fm4_ad`, `fm4_ud` are declared as `static` inside the switch at line 1704-1706 — unusual pattern
3. **`bool initializing`** (line 298) — Declared without `static` keyword but appears to function as global
4. **`bool boot_mode`** (line 339) — Declared without `static` keyword
5. **Duplicate `last_dist_solution` write** in renderFire lambda (line 1547) — writes to the static variable during base piece calculation
6. **`PI` macro** (line 211) — Redefines PI as a macro, could conflict with math headers
7. **Table loading** (lines 5061-5122) — Tries 3 different paths for tables.csv (cwd, backend/, exe dir)
8. **Zone detection** (lines 2416-2456) — Hardcoded weather zones for Honduras (Zambrano, Pinalejo, Trincheras)
9. **`drPrefix()`** (line 278) — Adds "DR EVIL" prefix only for MAIN and FM menus
10. **Firing table format** — CSV columns: ART, PROJ, CHG, DIST, QE, TOF, DRIFT, ANGLE (optional), VTERM (optional)

### 9.3 Hardcoded Values
- **Jump horizontal:** 6.6 mils (lines 1385, 1519)
- **Default def_base:** 3200 mils (line 240)
- **Default sheaf width:** 60.0 mils (line 229)
- **Wind/Honduras defaults:** wind_speed=2.5, temperature=28.0 (lines 118-119)
- **Latitude for Coriolis:** 14° (Honduras) (line 1047)
- **Spin drift constant:** 0.015 (line 1043)
- **Max trajectory steps:** 200,000 (line 935)
- **Integration dt:** 0.005s (line 922)
- **M107 mass:** 43.2 kg (line 793)
- **M107 caliber:** 0.155 m (line 794)

---

## 10. FILE SIZE SUMMARY

| File | Lines | Role |
|------|-------|------|
| basic_engine.cpp | 5126 | Core engine (ALL menu logic) |
| basic_engine.h | 179 | Engine header + Menu enum + state |
| ballistic_engine.cpp | 72 | Legacy ballistic solver |
| ballistic_engine.h | 16 | Legacy header |
| ballistic_tables.cpp | 62 | Legacy hardcoded tables |
| ballistic_tables.h | 25 | Legacy header |
| main.cpp | 260 | HTTP server + console |
| index.html | 526 | Web UI (calculator) |
| hp71b.html | 215 | Alternate HP-71B UI |
| map_model.cpp | 53 | Standalone map model (unused) |
| hardware_map.cpp | 1 | Stub |
| hp71_lcd.cpp | 16 | LCD emulation |
| user_auth.cpp | 16 | Auth (disabled) |
| keyboard.js | 31 | Keyboard handler |
| CMakeLists.txt | 7 | Top-level build |
| hp71_server/CMakeLists.txt | 31 | Server build |
