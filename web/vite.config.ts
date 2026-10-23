import { defineConfig, loadEnv } from "vite";
import type { Plugin, UserConfig } from "vite";

// The web build reads the Unreal project's own SourceData/ and Content/LFF/Crowd/ (one source of truth for both builds),
// so the dev server is allowed to serve from the repository root. Cesium's static files are copied into public/cesium by
// tools/prepare_assets.mjs, along with the transcoded audio and UI images.
const repo = new URL("..", import.meta.url).pathname;

// Link previews (iMessage, Slack, social sites) need absolute URLs for the page and its image. The Pages workflow passes
// the site's address as VITE_SITE_URL; local builds fall back to relative paths, which previews ignore.
function linkPreview(mode: string): Plugin {
  // loadEnv reads .env files and VITE_ variables already in the environment (the workflow's).
  const env = loadEnv(mode, new URL(".", import.meta.url).pathname, "VITE_");
  const site = (env.VITE_SITE_URL ?? "").replace(/\/*$/, "/");
  return {
    name: "lff-link-preview",
    transformIndexHtml: (html) => html.replaceAll("%SITE_URL%", site === "/" ? "" : site),
  };
}

export default defineConfig(({ mode }) => ({
  base: "./",
  plugins: [linkPreview(mode)],
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
}) as unknown as UserConfig);
