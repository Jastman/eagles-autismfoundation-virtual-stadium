// Gathers everything the web build serves as static files into web/public/ (git-ignored, regenerated on every dev/build):
//
//   public/cesium/   CesiumJS Workers, ThirdParty, Assets and Widgets (CESIUM_BASE_URL)
//   public/media/    audio from SourceData/Audio (AAC .m4a + MP3 when ffmpeg is installed, else the .wav), the
//                    voice word timings (*.words.json), SourceData/UI images and fonts, the crowd seat files from
//                    Content/LFF/Crowd, and the ticket frames
//
// The sources are the Unreal project's own files, so a change there reaches both builds. Ticket frames come from
// web/assets-src/ticket/*.png when Tools/Pipeline/export_web_assets.py has written them (full resolution); otherwise
// the editor thumbnails embedded in Content/LFF/UI/Ticket/*.uasset are used (128 x 256, a stand-in).
//
//   node tools/prepare_assets.mjs [--force]
import { execFileSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const web = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repo = resolve(web, "..");
const pub = join(web, "public");
const media = join(pub, "media");
const force = process.argv.includes("--force");

function newer(src, dst) {
    return force || !existsSync(dst) || statSync(src).mtimeMs > statSync(dst).mtimeMs;
}

function walk(dir, filter) {
    if (!existsSync(dir)) return [];
    return readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
        const path = join(dir, entry.name);
        return entry.isDirectory() ? walk(path, filter) : filter(path) ? [path] : [];
    });
}

function hasFfmpeg() {
    try {
        execFileSync("ffmpeg", ["-version"], { stdio: "ignore" });
        return true;
    } catch {
        return false;
    }
}

// 1. CesiumJS static files.
const cesiumBuild = join(web, "node_modules", "cesium", "Build", "Cesium");
for (const part of ["Workers", "ThirdParty", "Assets", "Widgets"]) {
    const dst = join(pub, "cesium", part);
    if (force || !existsSync(dst)) {
        cpSync(join(cesiumBuild, part), dst, { recursive: true });
    }
}

// 2. Audio. iPad Safari plays WAV, but 40 MB of it is slow over a venue network: AAC (iPad, Safari) and MP3 (every other
// browser, including test Chromium builds without AAC) are each about a tenth of the size. The player picks per browser.
const ffmpeg = hasFfmpeg();
const audioManifest = {};
mkdirSync(join(media, "audio"), { recursive: true });
const encodings = ffmpeg
    ? { m4a: ["-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart"], mp3: ["-c:a", "libmp3lame", "-b:a", "160k"] }
    : { wav: null };
for (const wav of walk(join(repo, "SourceData", "Audio"), (p) => p.endsWith(".wav"))) {
    const name = basename(wav, ".wav");
    audioManifest[name] = {};
    for (const [format, args] of Object.entries(encodings)) {
        const out = join(media, "audio", `${name}.${format}`);
        if (newer(wav, out)) {
            if (args) execFileSync("ffmpeg", ["-y", "-loglevel", "error", "-i", wav, ...args, out]);
            else cpSync(wav, out);
        }
        audioManifest[name][format] = `media/audio/${basename(out)}`;
    }
}
writeFileSync(join(media, "audio", "manifest.json"), JSON.stringify(audioManifest, null, 2));

// 3. Voice word timings (Tools/Audio/make_vo_words.py), the karaoke's clock.
mkdirSync(join(media, "vo"), { recursive: true });
for (const words of walk(join(repo, "SourceData", "Audio", "VO"), (p) => p.endsWith(".words.json"))) {
    cpSync(words, join(media, "vo", basename(words)));
}

// 4. UI images and the project font (Atkinson Hyperlegible, SIL OFL).
mkdirSync(join(media, "ui"), { recursive: true });
for (const file of walk(join(repo, "SourceData", "UI"), (p) => /\.(png|ttf|txt)$/.test(p))) {
    cpSync(file, join(media, "ui", basename(file)));
}
for (const file of walk(join(repo, "SourceData", "People"), (p) => p.endsWith(".png"))) {
    cpSync(file, join(media, "ui", basename(file)));
}

// 5. Crowd seats: the same files the Unreal crowd spawner reads.
mkdirSync(join(media, "crowd"), { recursive: true });
for (const name of ["seats.json", "seat_treads.json", "queue.json", "circulation.json"]) {
    const src = join(repo, "Content", "LFF", "Crowd", name);
    if (existsSync(src)) cpSync(src, join(media, "crowd", name));
}

// 5b. The billboard fans, drawn by the Unreal script's own code (tools/draw_crowd_fans.py; Python with Pillow). Without
// Python the crowd falls back to simpler cards drawn in the browser.
const fans = join(media, "crowd", "fans.png");
const fanSources = [join(web, "tools", "draw_crowd_fans.py"), join(repo, "Tools", "Editor", "build_crowd_billboards.py")];
if (force || !existsSync(fans) || fanSources.some((src) => statSync(src).mtimeMs > statSync(fans).mtimeMs)) {
    try {
        execFileSync(process.platform === "win32" ? "python" : "python3", [fanSources[0], fans], { stdio: ["ignore", "ignore", "pipe"] });
    } catch (error) {
        console.warn(`prepare_assets: crowd fans not drawn (needs Python 3 with Pillow): ${String(error.stderr ?? error).trim().split("\n").pop()}`);
    }
}

// 6. Ticket frames.
mkdirSync(join(media, "ticket"), { recursive: true });
const ticketFrames = [];
const exported = walk(join(web, "assets-src", "ticket"), (p) => p.endsWith(".png")).sort();
if (exported.length > 0) {
    for (const png of exported) {
        cpSync(png, join(media, "ticket", basename(png)));
        ticketFrames.push(`media/ticket/${basename(png)}`);
    }
} else {
    for (const uasset of walk(join(repo, "Content", "LFF", "UI", "Ticket"), (p) => p.endsWith(".uasset")).sort()) {
        const data = readFileSync(uasset);
        const start = data.indexOf(Buffer.from([0xff, 0xd8, 0xff]));
        const end = data.lastIndexOf(Buffer.from([0xff, 0xd9]));
        if (start < 0 || end < start) continue;
        const out = join(media, "ticket", basename(uasset, ".uasset") + ".jpg");
        writeFileSync(out, data.subarray(start, end + 2));
        ticketFrames.push(`media/ticket/${basename(out)}`);
    }
}
writeFileSync(join(media, "ticket", "manifest.json"), JSON.stringify({ frames: ticketFrames, fullResolution: exported.length > 0 }, null, 2));

// 6b. App shell files (web/static: manifest, icons) at the site root.
for (const file of walk(join(web, "static"), () => true)) cpSync(file, join(pub, basename(file)));

// 7. Optional models written by the Unreal/Blender export (web/assets-src/models/*.glb): the eagle, the stadium for local
// testing, props. Listed so the scene can prefer a model over its primitive stand-in.
mkdirSync(join(pub, "models"), { recursive: true });
const models = {};
for (const glb of walk(join(web, "assets-src", "models"), (p) => p.endsWith(".glb"))) {
    const rel = relative(join(web, "assets-src", "models"), glb).replaceAll("\\", "/");
    mkdirSync(dirname(join(pub, "models", rel)), { recursive: true });
    cpSync(glb, join(pub, "models", rel));
    models[rel.replace(/\.glb$/, "")] = `models/${rel}`;
}
writeFileSync(join(pub, "models", "manifest.json"), JSON.stringify(models, null, 2));

console.log(`prepare_assets: ${Object.keys(audioManifest).length} audio (${ffmpeg ? "AAC + MP3" : "WAV"}), ${ticketFrames.length} ticket frames` +
    `${exported.length > 0 ? "" : " (thumbnails)"}, ${Object.keys(models).length} models.`);
