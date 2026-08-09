# Santa Barbara - Calculadora de Tiro

Calculadora de Tiro para control de fuego de artillería, con modelo balístico STANAG 4355 para el Ejército de Honduras.

## Descripción

Santa Barbara es un sistema de cálculo balístico que utiliza tablas de tiro oficiales (Firing Tables) y el modelo STANAG 4355 para calcular ángulos de tiro, tiempos de vuelo y correcciones para artillería terrestre.

El sistema está diseñado para el M198 (L/39, obús remolcado de 155mm) y utiliza datos atmosféricos configurables para ajustar los cálculos a las condiciones locales.

## Características

- **Modelo balístico STANAG 4355** con tabla de arrastre M107
- **Firing Tables oficiales** FT 155-AM-2 C-5 y FT 105-AM-2
- **Atmósfera configurable** con corrección por temperatura y humedad
- **Modelo de viento** 2D con componente axial
- **Auto-detección de ubicación** desde coordenadas MAP MODEL
- **Interfaz HTTP** para integración con sistemas externos

## Arquitectura

```
hp71-standalone/
├── saturn_core/          # Motor balístico C++
│   ├── basic_engine.cpp  # Motor principal + menús
│   ├── basic_engine.h    # Definiciones
│   └── ballistic_engine.h # Ecuaciones de movimiento
├── hp71_server/          # Servidor HTTP
│   └── main.cpp          # Servidor
├── tables/               # Firing Tables oficiales
│   └── tables.csv        # FT 155mm y 105mm
└── web_ui/               # Interfaz web
```

## Modelo Balístico

### Tablas de Tiro (Firing Tables)
El sistema utiliza tablas de tiro oficiales del ejército:
- **FT 155-AM-2 C-5**: Tablas para obuses de 155mm (M109, M198, M777)
- **FT 105-AM-2**: Tablas para obuses de 105mm (M102)

Estas tablas contienen ángulos de tiro (QE), tiempos de vuelo, deriva y ángulos de impacto para diferentes distancias y cargas.

### Modelo STANAG 4355
El modelo STANAG 4355 utiliza:
- Tabla de arrastre G1 para el proyectil M107
- Ecuaciones de movimiento 2D con componente de viento
- Correcciones atmosféricas por temperatura y humedad

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
