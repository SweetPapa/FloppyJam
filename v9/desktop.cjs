const { app, BrowserWindow, ipcMain, session } = require("electron");
const path = require("node:path");
let main;
app.whenReady().then(() => {
  main = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 800,
    minHeight: 600,
    backgroundColor: "#040910",
    title: "AFTERGLOW",
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  main.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
  main.webContents.on("will-navigate", (event) => event.preventDefault());
  session.defaultSession.setPermissionRequestHandler(
    (_wc, _permission, callback) => callback(false),
  );
  ipcMain.handle("model-request", async (event, payload) => {
    if (
      event.sender !== main.webContents ||
      !event.senderFrame.url.startsWith("file://")
    )
      throw new Error("Invalid sender");
    try {
      const url = new URL(payload.url);
      if (
        !["http:", "https:"].includes(url.protocol) ||
        url.username ||
        url.password ||
        !url.pathname.endsWith("/chat/completions")
      )
        throw new Error("Invalid endpoint");
      const body = JSON.stringify(payload.body);
      if (body.length > 40000) throw new Error("Request too large");
      const response = await fetch(url, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          ...(payload.headers?.Authorization
            ? { Authorization: String(payload.headers.Authorization) }
            : {}),
        },
        body,
        redirect: "error",
        signal: AbortSignal.timeout(8000),
      });
      const reader = response.body.getReader(),
        decoder = new TextDecoder();
      let text = "";
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        text += decoder.decode(value, { stream: true });
        if (text.length > 65536) {
          await reader.cancel();
          throw new Error("Response too large");
        }
      }
      text += decoder.decode();
      return {
        ok: response.ok,
        status: response.status,
        type: response.headers.get("content-type") || "",
        text,
      };
    } catch {
      return { ok: false, status: 0, type: "", text: "" };
    }
  });
  main.loadFile(path.join(__dirname, "dist", "index.html"));
});
app.on("window-all-closed", () => app.quit());
