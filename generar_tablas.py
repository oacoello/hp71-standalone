import os
import csv

OUTPUT_PATH = r"C:\Users\dolfo\Documents\hp71_emulator\tables\tables.csv"

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)


def write_row(writer, art, proj, chg, dist, qe, tof, drift):
    writer.writerow([
        int(art),
        str(proj),
        int(chg),
        int(round(dist)),
        round(float(qe), 1),
        round(float(tof), 4),
        round(float(drift), 4)
    ])


def clamp(value, low, high):
    return max(low, min(high, value))


def smoothstep(edge0, edge1, x):
    if edge0 == edge1:
        return 0.0

    t = (x - edge0) / (edge1 - edge0)
    t = clamp(t, 0.0, 1.0)

    return t * t * (3.0 - 2.0 * t)


def chg6_low_curve_factor(art, proj, chg, dist):
    art = int(art)
    proj = str(proj).upper()
    chg = int(chg)

    if art != 155:
        return 0.0

    if proj not in ("HEA", "M4A2"):
        return 0.0

    if chg != 6:
        return 0.0

    start = 9000.0
    full = 9300.0
    fade_start = 9600.0
    end = 10000.0

    if dist < start or dist >= end:
        return 0.0

    if dist < full:
        return smoothstep(start, full, dist)

    if dist <= fade_start:
        return 1.0

    return 1.0 - smoothstep(fade_start, end, dist)


def chg6_mid_curve_factor(art, proj, chg, dist):
    art = int(art)
    proj = str(proj).upper()
    chg = int(chg)

    if art != 155:
        return 0.0

    if proj not in ("HEA", "M4A2"):
        return 0.0

    if chg != 6:
        return 0.0

    start = 9900.0
    full_start = 10050.0
    full_end = 10300.0
    end = 10400.0

    if dist < start or dist >= end:
        return 0.0

    if dist < full_start:
        return smoothstep(start, full_start, dist)

    if dist <= full_end:
        return 1.0

    return 1.0 - smoothstep(full_end, end, dist)


def chg6_near_mid_curve_factor(art, proj, chg, dist):
    art = int(art)
    proj = str(proj).upper()
    chg = int(chg)

    if art != 155:
        return 0.0

    if proj not in ("HEA", "M4A2"):
        return 0.0

    if chg != 6:
        return 0.0

    start = 9900.0
    full_start = 10000.0
    full_end = 10100.0
    end = 10200.0

    if dist < start or dist >= end:
        return 0.0

    if dist < full_start:
        return smoothstep(start, full_start, dist)

    if dist <= full_end:
        return 1.0

    return 1.0 - smoothstep(full_end, end, dist)


def add_table_block(
    writer,
    art,
    proj,
    chg,
    dmin,
    dmax,
    step,
    base_scale,
    chg_scale,
    curve_scale,
    tof_scale,
    drift_scale,
    power=1.0,
    qe_bias=0.0,
    tof_bias=0.0,
    drift_bias=0.0
):
    for d in range(dmin, dmax + 1, step):
        km = d / 1000.0

        qe = ((base_scale * km) + (curve_scale * km * km) + (chg * chg_scale)) / power
        qe += qe_bias

        tof = (d / (tof_scale * power)) + (chg * 0.16) + tof_bias

        drift = ((d / 10000.0) * chg * drift_scale) + drift_bias

        low_factor = chg6_low_curve_factor(art, proj, chg, d)

        qe *= 1.0 - (0.075 * low_factor)
        tof *= 1.0 - (0.135 * low_factor)
        drift *= 1.0 - (0.850 * low_factor)

        if drift < 0.0:
            drift = 0.0

        write_row(writer, art, proj, chg, d, qe, tof, drift)


def add_105_projectile_family(
    writer,
    proj,
    drift_scale=1.00,
    power=1.00,
    qe_bias=0.0,
    tof_bias=0.0,
    drift_bias=0.0
):
    """
    Genera una familia completa 105mm para cargas 4, 5, 6 y 7.

    Esta funcion no fuerza resultados por ejercicio.
    Solo aplica parametros generales a toda la familia del proyectil 105mm.

    No toca 155mm.
    """

    add_table_block(
        writer, 105, proj, 4,
        1000, 6000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=drift_scale,
        power=power,
        qe_bias=qe_bias,
        tof_bias=tof_bias,
        drift_bias=drift_bias
    )

    add_table_block(
        writer, 105, proj, 5,
        1500, 7500, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=drift_scale,
        power=power,
        qe_bias=qe_bias,
        tof_bias=tof_bias,
        drift_bias=drift_bias
    )

    add_table_block(
        writer, 105, proj, 6,
        2000, 9000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=drift_scale,
        power=power,
        qe_bias=qe_bias,
        tof_bias=tof_bias,
        drift_bias=drift_bias
    )

    add_table_block(
        writer, 105, proj, 7,
        3000, 10000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=drift_scale,
        power=power,
        qe_bias=qe_bias,
        tof_bias=tof_bias,
        drift_bias=drift_bias
    )


def add_105_hea_family(writer):
    """
    Genera HEA 105mm como familia completa.

    Cambio aplicado:
    - Antes HEA estaba castigado con power=1.06 y qe_bias=-2.0.
    - Eso bajaba todo el QE de HEA en 105mm.
    - Ahora HEA queda alineado al modelo base 105, sin forzar una carga,
      sin forzar una distancia y sin tocar 155mm.
    """

    add_table_block(
        writer, 105, "HEA", 4,
        1000, 7000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=0.96,
        power=1.00,
        qe_bias=0.0,
        tof_bias=0.0
    )

    add_table_block(
        writer, 105, "HEA", 5,
        1500, 8500, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=0.96,
        power=1.00,
        qe_bias=0.0,
        tof_bias=0.0
    )

    add_table_block(
        writer, 105, "HEA", 6,
        2000, 10000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=0.96,
        power=1.00,
        qe_bias=0.0,
        tof_bias=0.0
    )

    add_table_block(
        writer, 105, "HEA", 7,
        3000, 11500, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=0.96,
        power=1.00,
        qe_bias=0.0,
        tof_bias=0.0
    )


def add_105_ila_family(writer):
    """
    Genera ILA 105mm como familia propia.
    Se mantiene igual que antes para no dañar lo existente.
    """

    add_table_block(
        writer, 105, "ILA", 4,
        1000, 5000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=1.04,
        power=0.96,
        qe_bias=3.0,
        tof_bias=0.10
    )

    add_table_block(
        writer, 105, "ILA", 5,
        1500, 6500, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=1.04,
        power=0.96,
        qe_bias=3.0,
        tof_bias=0.10
    )

    add_table_block(
        writer, 105, "ILA", 6,
        2000, 8000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=1.04,
        power=0.96,
        qe_bias=3.0,
        tof_bias=0.10
    )

    add_table_block(
        writer, 105, "ILA", 7,
        3000, 9000, 100,
        base_scale=38.0,
        chg_scale=10.5,
        curve_scale=0.42,
        tof_scale=285.0,
        drift_scale=1.04,
        power=0.96,
        qe_bias=3.0,
        tof_bias=0.10
    )


def add_105_family(writer):
    """
    Familia completa 105mm.

    Se mantienen todas las familias existentes:
    - HE
    - M1
    - M67
    - HEA
    - ILA

    No se elimina ningun PROJ.
    No se fuerza ninguna carga.
    No se fuerza ninguna distancia.
    No se toca 155mm.
    """

    for proj in ["HE", "M1", "M67"]:
        add_105_projectile_family(
            writer,
            proj,
            drift_scale=1.00,
            power=1.00,
            qe_bias=0.0,
            tof_bias=0.0,
            drift_bias=0.0
        )

    add_105_hea_family(writer)

    add_105_ila_family(writer)


def add_155_family(writer):
    add_table_block(
        writer, 155, "HE", 4,
        3000, 9000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.00,
        power=1.00
    )

    add_table_block(
        writer, 155, "HE", 5,
        5000, 10000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.00,
        power=1.00
    )

    add_table_block(
        writer, 155, "HE", 6,
        6000, 18000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.00,
        power=1.00
    )

    add_table_block(
        writer, 155, "HE", 7,
        8000, 20000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.00,
        power=1.00
    )

    add_table_block(
        writer, 155, "HEA", 5,
        5000, 8500, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=0.94,
        power=1.08,
        qe_bias=-5.0,
        tof_bias=-0.30
    )

    add_table_block(
        writer, 155, "HEA", 6,
        6000, 19000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.30,
        tof_scale=252.0,
        drift_scale=0.6267,
        power=1.11,
        qe_bias=-38.0,
        tof_bias=-1.20
    )

    add_table_block(
        writer, 155, "HEA", 7,
        8000, 24000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=0.94,
        power=1.08,
        qe_bias=-5.0,
        tof_bias=-0.30
    )

    add_table_block(
        writer, 155, "M4A2", 5,
        5000, 8500, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=0.94,
        power=1.08,
        qe_bias=-5.0,
        tof_bias=-0.30
    )

    add_table_block(
        writer, 155, "M4A2", 6,
        6000, 19000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.30,
        tof_scale=252.0,
        drift_scale=0.6267,
        power=1.11,
        qe_bias=-38.0,
        tof_bias=-1.20
    )

    add_table_block(
        writer, 155, "M4A2", 7,
        8000, 24000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=0.94,
        power=1.08,
        qe_bias=-5.0,
        tof_bias=-0.30
    )

    add_table_block(
        writer, 155, "ILA", 4,
        3000, 8000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.05,
        power=0.95,
        qe_bias=5.0,
        tof_bias=0.30
    )

    add_table_block(
        writer, 155, "ILA", 5,
        4000, 10000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.05,
        power=0.95,
        qe_bias=5.0,
        tof_bias=0.30
    )

    add_table_block(
        writer, 155, "ILA", 6,
        5000, 15000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.05,
        power=0.95,
        qe_bias=5.0,
        tof_bias=0.30
    )

    add_table_block(
        writer, 155, "ILA", 7,
        6000, 18000, 100,
        base_scale=39.5,
        chg_scale=13.2,
        curve_scale=0.32,
        tof_scale=252.0,
        drift_scale=1.05,
        power=0.95,
        qe_bias=5.0,
        tof_bias=0.30
    )


def main():
    with open(OUTPUT_PATH, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["ART", "PROJ", "CHG", "DIST", "QE", "TOF", "DRIFT"])

        add_105_family(writer)
        add_155_family(writer)

    print("tables.csv generado correctamente")
    print("Salida:", OUTPUT_PATH)


if __name__ == "__main__":
    main()