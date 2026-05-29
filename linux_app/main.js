const { app, BrowserWindow } = require("electron");
const { spawn } = require("child_process");
const path = require("path");
const fs = require("fs");
const http = require("http");

let backend = null;
let splash = null;
let mainWindow = null;
let uiServer = null;

const UI_PORT = 18080;

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
        path.join(root, "backend", "hp71_emulator.exe"),
        path.join(root, "hp71_emulator.exe"),
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

    return backendDir;
}

function getContentType(filePath) {
    const ext = path.extname(filePath).toLowerCase();

    if (ext === ".html") return "text/html; charset=utf-8";
    if (ext === ".js") return "application/javascript; charset=utf-8";
    if (ext === ".css") return "text/css; charset=utf-8";
    if (ext === ".csv") return "text/csv; charset=utf-8";
    if (ext === ".png") return "image/png";
    if (ext === ".gif") return "image/gif";

    return "application/octet-stream";
}

function proxyInput(req, res) {
    const proxyReq = http.request({
        hostname: "127.0.0.1",
        port: 8080,
        path: "/input",
        method: "POST",
        headers: req.headers
    }, (proxyRes) => {
        res.writeHead(proxyRes.statusCode || 500, proxyRes.headers);
        proxyRes.pipe(res);
    });

    proxyReq.on("error", () => {
        res.writeHead(502, { "Content-Type": "text/plain; charset=utf-8" });
        res.end("ERROR: backend no disponible");
    });

    req.pipe(proxyReq);
}

function startUiServer(backendDir) {
    uiServer = http.createServer((req, res) => {
        const requestUrl = new URL(req.url, `http://127.0.0.1:${UI_PORT}`);

        if (requestUrl.pathname === "/input" && req.method === "POST") {
            proxyInput(req, res);
            return;
        }

        const requestedPath = requestUrl.pathname === "/"
            ? "index.html"
            : decodeURIComponent(requestUrl.pathname).replace(/^\/+/, "");

        const filePath = path.resolve(backendDir, requestedPath);
        const backendRoot = path.resolve(backendDir);

        if (!filePath.startsWith(backendRoot) || !fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
            res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
            res.end("Not found");
            return;
        }

        res.writeHead(200, { "Content-Type": getContentType(filePath) });
        fs.createReadStream(filePath).pipe(res);
    });

    return new Promise((resolve, reject) => {
        uiServer.once("error", reject);
        uiServer.listen(UI_PORT, "127.0.0.1", resolve);
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

    mainWindow.loadURL(`http://127.0.0.1:${UI_PORT}`);

    mainWindow.once("ready-to-show", () => {
        setTimeout(() => {
            if (splash && !splash.isDestroyed()) {
                splash.destroy();
            }

            mainWindow.show();
        }, 3200);
    });
}

app.whenReady().then(async () => {
    const backendDir = startBackend();
    createSplash();
    await startUiServer(backendDir);

    setTimeout(() => {
        createMainWindow();
    }, 1200);
});

app.on("window-all-closed", () => {
    if (uiServer) {
        uiServer.close();
        uiServer = null;
    }

    if (backend) {
        backend.kill();
        backend = null;
    }

    app.quit();
});
