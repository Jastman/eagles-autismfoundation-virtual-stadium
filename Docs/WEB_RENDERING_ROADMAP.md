# Web rendering roadmap: three.js and other options

The web build draws everything with CesiumJS: the Google city tiles, the licensed stadium (3D Tiles on Cesium ion), the
level's props, crowd, cues, signs and now the night lighting, LED boards and light show (WEB_PARITY W21-W23). CesiumJS
is the right base for the georeferenced, streamed parts. Where it is weakest is real-time "game engine" lighting and
effects: many dynamic lights, bloom that doesn't halo dark edges (W15), particles, and animated characters. This note
lists what would make the experience look closer to the Unreal build, in order of value for effort.

## Status (October 23, 2026)

Done (WEB_PARITY W24): the three.js layer and its camera bridge, bloom, the floodlight glare and beams, the phone light
show and the fireworks and gate flares as GPU particles, and a CustomShader that floodlights the stadium model by normal.
Not done: animated people (no skinned, clothed people can be exported from the MetaHumans: their outfits are Chaos cloth;
it needs a separate set of rigged glTF fans), real shadow-casting lights in three.js (they can't light Cesium's tiles,
so the CustomShader does that job), video textures for the boards (needs real board loops), and the Cesium-side items
below.

## 1. A three.js layer over Cesium

Run three.js in the same page, rendering into its own transparent canvas on top of Cesium's (or into Cesium's WebGL2
context), with its camera copied from Cesium's every frame (`camera.positionWC`, `directionWC`, `upWC`, frustum) and its
scene placed in the stadium's ENU frame (`Geo.enuToFixed`). Depth: read Cesium's depth or keep three.js content
(people, effects) in front of the bowl, where it always is.

What it buys, for this project:

- **Floodlights as real lights.** Dozens of spot lights with shadows on the field and stands, and volumetric light cones
  above the bowl (`three/examples` volumetric spot light shader), instead of a position-based relight.
- **Selective bloom** (`UnrealBloomPass` with a layer mask): the light banks, LED boards and phones glow without the
  dark-edge halos Cesium's whole-picture bloom gives (W15).
- **People that move.** Skinned glTF crowds with `InstancedMesh` / `BatchedMesh` and baked animation textures, for the
  plaza, lines and concourse walkers that are stand-ins or cards today (W4, G2).
- **GPU particles** for the fireworks, gate flares and the phone light show, with soft additive sprites.
- **Video textures** (`THREE.VideoTexture`) for the boards, if the Eagles supply real board loops.

Cost: two renderers' memory on an iPad (keep three.js content small; Safari closes heavy tabs, W16), and a second
camera to keep in step. A proof of concept is about a week: the camera bridge, one floodlight rig and selective bloom.

## 2. Inside CesiumJS, without a second renderer

- **CustomShader on the stadium tilesets** for per-surface floodlight lighting with normals (better than today's
  position-only post-process), and emissive LED materials on the model's own display surfaces.
- **Gaussian splats** of the real concourses, plaza and sensory room (Cesium ion now hosts 3D Tiles splats) for
  photographic interiors where the model has none.
- **KTX2 stadium textures** (W20) to free memory for any of the above.

## 3. Other modern web tools

- **Theatre.js** or GSAP for authored, scrubbable camera moves and timed cues (the anthem sequence, fireworks), editable
  without code.
- **WebGPU** (three.js `WebGPURenderer`, Babylon.js) once iPad Safari's WebGPU support is dependable: compute-driven
  crowds and particles at a fraction of the CPU cost.
- **React Three Fiber** only if the HUD moves to React; the HUD is plain TypeScript today and works well.

## Comfort rules still apply

Whatever is added follows the project's rules (ACCESSIBILITY.md): nothing strobes or flashes, Calm Mode and less
movement hold effects still or hide them, and every effect stays optional to the tour.
