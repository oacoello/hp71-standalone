const { app, BrowserWindow } = require("electron");
const { spawn } = require("child_process");
const path = require("path");
const fs = require("fs");

let backend = null;
let splash = null;
let mainWindow = null;

app.disableHardwareAcceleration();
app.commandLine.appendSwitch("disable-gpu");
app.commandLine.appendSwitch("disable-software-rasterizer");

function getAppRoot() {
    if (app.isPackaged) {
        return process.resourcesPath;
    }

    return path.join(__dirname, "..");
}

function startBackend() {
    const root = getAppRoot();

    const candidates = [
        path.join(root, "backend", "hp71_emulator"),
        path.join(root, "hp71_emulator"),
        path.join(root, "build-linux", "hp71_server", "hp71_emulator")
    ];

    let backendPath = null;

    for (const p of candidates) {
        if (fs.existsSync(p)) {
            backendPath = p;
            break;
        }
    }

    if (!backendPath) {
        throw new Error("No se encontro hp71_emulator");
    }

    const backendDir = path.dirname(backendPath);

    backend = spawn(backendPath, [], {
        cwd: backendDir,
        stdio: "ignore",
        detached: false
    });

    backend.on("error", (err) => {
        console.error("Backend error:", err);
    });
}

function createSplash() {
    splash = new BrowserWindow({
        width: 780,
        height: 524,
        frame: false,
        alwaysOnTop: true,
        center: true,
        resizable: false,
        backgroundColor: "#000000",
        webPreferences: {
            contextIsolation: true,
            nodeIntegration: false
        }
    });

    splash.loadFile(path.join(__dirname, "splash.html"));
}

function createMainWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        show: false,
        center: true,
        backgroundColor: "#000000",
        webPreferences: {
            contextIsolation: true,
            nodeIntegration: false
        }
    });

    mainWindow.loadURL("http://127.0.0.1:8080");

    mainWindow.once("ready-to-show", () => {
        setTimeout(() => {
            if (splash && !splash.isDestroyed()) {
                splash.destroy();
            }

            mainWindow.show();
        }, 3200);
    });
}

app.whenReady().then(() => {
    startBackend();
    createSplash();

    setTimeout(() => {
        createMainWindow();
    }, 1200);
});

app.on("window-all-closed", () => {
    if (backend) {
        backend.kill();
        backend = null;
    }

    app.quit();
});
