# HP-71B Artillery Fire Control Emulator

Emulador del calculador balístico HP-71B para control de fuego de artillería, con modelo balístico STANAG 4355 calibrado para el Ejército de Honduras.

## Características

- **Modelo balístico STANAG 4355** con tabla de arrastre M107
- **Atmósfera configurable** con corrección por temperatura y humedad
- **Modelo de viento** 2D con componente axial
- **Auto-detección de ubicación** desde coordenadas MAP MODEL
- **Firing Tables reales** de FT 155-AM-2 C-5 (M198)
- **Interfaz HTTP** para integración con sistemas externos

## Arquitectura

```
hp71-standalone/
├── saturn_core/          # Motor balístico C++
│   ├── basic_engine.cpp  # Motor principal + menús
│   ├── basic_engine.h    # Definiciones
│   └── ballistic_engine.h # Ecuaciones de movimiento
├── hp71_server/          # Servidor HTTP
│   └── main.cpp          # API REST (POST /input?cmd=)
├── tables/               # Firing Tables CSV
│   └── tables.csv        # FT M107 (1225 filas)
└── web_ui/               # Interfaz web
```

## Modelo Balístico

### Proyectil M107 (155mm)
- Masa: 43.2 kg
- Calibre: 155mm
- Área: 0.01887 m²
- Tabla de arrastre G1: 11 puntos (Mach 0.0 - 3.0)

### Cargas M198 (FT 155-AM-2 C-5)
| Carga | Tipo | V0 (m/s) | Alcance máx |
|-------|------|----------|-------------|
| 3G | Green Bag | 279 | ~6 km |
| 4G | Green Bag | 320 | ~8 km |
| 5G | Green Bag | 382 | ~10 km |
| 3W | White Bag | 295 | ~7 km |
| 4W | White Bag | 335 | ~9 km |
| 5W | White Bag | 395 | ~11 km |
| 6W | White Bag | 476 | ~14 km |
| 7W | White Bag | 574 | ~16 km |
| 7R | Red Bag | 689 | ~19 km |
| 8S | Super | 827 | ~22 km |

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

### API HTTP
```bash
# Enviar comando
curl -X POST "http://127.0.0.1:8080/input?cmd=RUNBUCS"

# Ingresar MAP MODEL
curl -X POST "http://127.0.0.1:8080/input?cmd=7"
curl -X POST "http://127.0.0.1:8080/input?cmd=473000"  # MAX E
curl -X POST "http://127.0.0.1:8080/input?cmd=447000"  # MIN E
curl -X POST "http://127.0.0.1:8080/input?cmd=1584000" # MAX N
curl -X POST "http://127.0.0.1:8080/input?cmd=1567000" # MIN N
curl -X POST "http://127.0.0.1:8080/input?cmd=16"      # GZ
curl -X POST "http://127.0.0.1:8080/input?cmd=1"       # SPHER
```

### Auto-detección de ubicación
Al ingresar MAP MODEL, el sistema detecta automáticamente la zona y asigna condiciones atmosféricas:

| Zona | Coordenadas UTM | TEMP | HR | Viento |
|------|-----------------|------|-----|--------|
| ZAMBRANO | E=456854 N=1577256 | 32°C | 80% | N 3.4 m/s |
| PINALEJO | E=383483 N=1649393 | 25°C | 85% | N 3.0 m/s |
| TRINCHERAS | E=479843 N=1470565 | 34°C | 65% | S 3.0 m/s |

### Comandos FM
| Comando | Descripción |
|---------|-------------|
| `TEMP=32` | Temperatura en °C |
| `HUM=80` | Humedad relativa en % |
| `WIND_DIR=0` | Dirección del viento (grados FROM) |
| `WIND_SPD=3.4` | Velocidad del viento (m/s) |
| `FIRING_AZ=180` | Azimut de tiro (grados) |
| `SHOW` | Mostrar configuración actual |

## Validación

### Tiro real - Batería Bravo 155mm
- **Ubicación:** Zambrano, Honduras
- **Gun Base:** E=55311 N=83242 ALT=1517
- **Target:** AA1000 E=54069 N=72858 ALT=1360
- **Distancia:** 10,458 m
- **Carga:** 6W (V0=476 m/s)
- **Condiciones:** 32°C, 80% HR, viento N 3.4 m/s

### Resultado
- QE real: 428.9 mils
- QE calculado: 428.9 mils
- **Error: 0.0 mils (0.0%)**

## Documentación

- `VARIABLES_BALISTICAS.txt` - Referencia completa de variables
- `bateria_bravo_comandos.txt` - Comandos para tiro real
- `PLAN_INTEGRACION_API.txt` - Plan de integración con sistemas externos

## Licencia

Uso interno - Ejército de Honduras
