const { contextBridge, ipcRenderer } = require("electron");
contextBridge.exposeInMainWorld("afterglowDesktop", {
  request: (payload) => ipcRenderer.invoke("model-request", payload),
});
