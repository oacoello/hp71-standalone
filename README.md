# HP-71B Artillery Fire Control Emulator

Emulador del calculador balístico HP-71B para control de fuego de artillería, con modelo balístico STANAG 4355 para el Ejército de Honduras.

## Características

- **Modelo balístico STANAG 4355** con tabla de arrastre M107
- **Atmósfera configurable** con corrección por temperatura y humedad
- **Modelo de viento** 2D con componente axial
- **Auto-detección de ubicación** desde coordenadas MAP MODEL
- **Firing Tables reales** de FT 155-AM-2 C-5

## Arquitectura

```
hp71-standalone/
├── saturn_core/          # Motor balístico C++
│   ├── basic_engine.cpp  # Motor principal + menús
│   ├── basic_engine.h    # Definiciones
│   └── ballistic_engine.h # Ecuaciones de movimiento
├── hp71_server/          # Servidor HTTP
│   └── main.cpp          # Servidor
├── tables/               # Firing Tables CSV
│   └── tables.csv        # FT M107
└── web_ui/               # Interfaz web
```

## Modelo Balístico

### Proyectil M107 (155mm)
- Masa: 43.2 kg
- Calibre: 155mm
- Área: 0.01887 m²
- Tabla de arrastre G1: 11 puntos (Mach 0.0 - 3.0)

### Atmósfera
- Modelo ISA con corrección por temperatura (TEMP=)
- Corrección por humedad relativa (HUM=)
- Modelo de viento 2D (WIND_DIR=, WIND_SPD=)

## Instalación

### Requisitos
- C++17 o superior
- CMake 3.10+
- Windows (para build completo)

### Build
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Ejecutar servidor
```bash
cd build/hp71_server/Release
./hp71_emulator.exe
```

El servidor inicia en `http://127.0.0.1:8080`

## Uso

### Auto-detección de ubicación
Al ingresar MAP MODEL, el sistema detecta automáticamente la zona y asigna condiciones atmosféricas predefinidas.

### Comandos FM
| Comando | Descripción |
|---------|-------------|
| `TEMP=` | Temperatura en °C |
| `HUM=` | Humedad relativa en % |
| `WIND_DIR=` | Dirección del viento (grados FROM) |
| `WIND_SPD=` | Velocidad del viento (m/s) |
| `FIRING_AZ=` | Azimut de tiro (grados) |
| `SHOW` | Mostrar configuración actual |

## Documentación

- `VARIABLES_BALISTICAS.txt` - Referencia completa de variables
- `bateria_bravo_comandos.txt` - Comandos para tiro real

## Licencia

Uso interno - Ejército de Honduras
