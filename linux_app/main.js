const { app, BrowserWindow } = require("electron");
const { spawn } = require("child_process");
const path = require("path");
const fs = require("fs");
const http = require("http");

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

    console.log("Backend path:", backendPath);
    console.log("Backend dir:", backendDir);

    backend = spawn(backendPath, [], {
        cwd: backendDir,
        stdio: "inherit",
        detached: false
    });

    backend.on("error", (err) => {
        console.error("Backend error:", err);
    });

    backend.on("exit", (code, signal) => {
        console.error("Backend exit:", code, signal);
    });
}

function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function waitForBackend(timeoutMs = 12000) {
    const started = Date.now();

    return new Promise((resolve, reject) => {
        function check() {
            const req = http.request({
                hostname: "127.0.0.1",
                port: 8080,
                path: "/",
                method: "GET",
                timeout: 1000
            }, (res) => {
                res.resume();
                resolve();
            });

            req.on("error", () => {
                if (Date.now() - started > timeoutMs) {
                    reject(new Error("Backend no respondio en 8080"));
                } else {
                    setTimeout(check, 300);
                }
            });

            req.on("timeout", () => {
                req.destroy();

                if (Date.now() - started > timeoutMs) {
                    reject(new Error("Timeout esperando backend 8080"));
                } else {
                    setTimeout(check, 300);
                }
            });

            req.end();
        }

        check();
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
        if (splash && !splash.isDestroyed()) {
            splash.destroy();
        }

        mainWindow.show();
    });

    mainWindow.webContents.on("did-fail-load", (event, errorCode, errorDescription) => {
        console.error("Main window failed load:", errorCode, errorDescription);
    });
}

app.whenReady().then(async () => {
    try {
        startBackend();
        createSplash();

        await Promise.all([
            waitForBackend(),
            delay(3200)
        ]);

        createMainWindow();
    } catch (err) {
        console.error(err);

        if (splash && !splash.isDestroyed()) {
            splash.destroy();
        }

        app.quit();
    }
});

app.on("window-all-closed", () => {
    if (backend) {
        backend.kill();
        backend = null;
    }

    app.quit();
});
