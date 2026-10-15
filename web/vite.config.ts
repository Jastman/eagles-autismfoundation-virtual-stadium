import { defineConfig } from "vite";

// The web build reads the Unreal project's own SourceData/ and Content/LFF/Crowd/ (one source of truth for both builds),
// so the dev server is allowed to serve from the repository root. Cesium's static files are copied into public/cesium by
// tools/prepare_assets.mjs, along with the transcoded audio and UI images.
const repo = new URL("..", import.meta.url).pathname;

export default defineConfig({
  base: "./",
  define: {
    CESIUM_BASE_URL: JSON.stringify("./cesium"),
  },
  server: {
    host: true,
    fs: { allow: [repo] },
  },
  build: {
    target: "es2022",
    chunkSizeWarningLimit: 6000,
    assetsInlineLimit: 0,
  },
  test: {
    include: ["test/**/*.test.ts"],
    environment: "node",
  },
} as Parameters<typeof defineConfig>[0]);
