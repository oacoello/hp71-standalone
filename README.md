# Santa Barbara FDC — HP-71 Standalone

Calculadora artillera standalone basada en un emulador HP-71B, empaquetada como aplicación de escritorio con Electron.

La aplicación incluye una interfaz local para cálculo balístico y un backend nativo `hp71_emulator` que procesa los comandos del sistema.

## Descargas

Los instaladores oficiales están publicados en GitHub Releases:

[Descargar Santa Barbara FDC v1.0.1](https://github.com/oacoello/hp71-standalone/releases/tag/v1.0.1)

### Windows

Descargá e instalá:

```text
Santa.Barbara.FDC.Setup.1.0.1.exe
```

### Linux Debian/Ubuntu

Descargá:

```text
santa-barbara-fdc_1.0.1_amd64.deb
```

Instalación:

```bash
sudo dpkg -i santa-barbara-fdc_1.0.1_amd64.deb
sudo apt-get install -f
```

## Cómo funciona

La app usa dos piezas:

- **Electron UI**: muestra la interfaz de la calculadora y el splash animado.
- **Backend HP-71B**: ejecuta `hp71_emulator` y procesa los comandos balísticos.

En ejecución local:

- La interfaz se sirve en `127.0.0.1:18080`.
- El backend balístico corre en `127.0.0.1:8080`.
- Electron redirige las llamadas `/input` desde la UI hacia el backend.

## Desarrollo

Requisitos principales:

- Node.js / npm
- Electron Builder
- CMake y compilador C++ si se recompila el backend
- WSL con `dpkg-deb` para generar el paquete Linux desde Windows

Instalar dependencias Electron:

```bash
cd linux_app
npm install
```

Generar paquete Windows:

```bash
npm run dist:win
```

Generar paquete Linux:

```bash
npm run pack:linux
wsl bash "/mnt/c/Users/oacoe/My Docs/Projects/hp71-standalone/linux_app/scripts/build-deb-from-unpacked.sh"
```

## Estructura principal

```text
hp71_server/          Backend HTTP del emulador HP-71B
saturn_core/          Núcleo de emulación y motor balístico
electron_payload/     Archivos servidos por la app empaquetada
linux_app/            Aplicación Electron y scripts de empaquetado
tables/               Tablas balísticas
web_ui/               Interfaz web base
```

## Notas

- Los instaladores no se versionan dentro del repositorio; se publican como assets en GitHub Releases.
- `node_modules`, builds temporales, reportes locales y datos sensibles están excluidos por `.gitignore`.
- El instalador Windows no está firmado digitalmente, por lo que Windows SmartScreen puede mostrar advertencias.

## Licencia

UNLICENSED.
