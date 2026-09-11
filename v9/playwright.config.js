import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "tests",
  testMatch: "*.spec.js",
  timeout: 240000,
  workers: 1,
  use: {
    baseURL: "http://127.0.0.1:5199",
    viewport: { width: 1440, height: 900 },
    launchOptions: {
      ...(process.env.PW_CHANNEL ? { channel: process.env.PW_CHANNEL } : {}),
      args: [
        "--enable-webgl",
        "--ignore-gpu-blocklist",
        "--enable-unsafe-swiftshader",
      ],
    },
    screenshot: "only-on-failure",
  },
  webServer: {
    command: "npm run dev",
    url: "http://127.0.0.1:5199",
    reuseExistingServer: !process.env.CI,
  },
  reporter: [["list"]],
});
