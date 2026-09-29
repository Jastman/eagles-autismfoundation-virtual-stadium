# Performance baseline

Evidence rule (LEDGER D5): every number here comes from a repeatable CLI capture. Keep the CSV under `Saved/Profiling/`, which isn't committed.

## How to capture
Close the editor first, so the two processes don't compete for the GPU. Then run:

```bash
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/VirtualStadium.uproject" \
  -game -windowed -ResX=1920 -ResY=1080 -ModelContextProtocolPort=18000 \
  -csvCaptureFrames=1200 -csvGpuStats -csvExitOnCompletion -nosplash -log
```

- The game logs to `~/Library/Logs/VirtualStadium/`.
- The CSV lands in `~/Library/Application Support/Epic/UnrealEngine/5.8/Saved/Profiling/CSV/`.
- In 5.8.3 the game **doesn't exit** after `-csvExitOnCompletion`, so stop it once "Capture Ended" is logged.
- Analyse only the second half of the frames, after tiles and Nanite have streamed in.

## 2026-09-29: Mac (M4 Max, 32 GPU cores), UE 5.8.3, official model (ADR-0004)
The scene is the intro view (CT_ExteriorArrival): the full CGTrader stadium (6.9 M triangles, Nanite), the clipped Google tiles and the cinematic look. That means Lumen GI and reflections at quality 2, virtual shadow maps and TSR.

| Metric (steady state) | Mean | p50 | p95 | Max |
|---|---|---|---|---|
| Frame time | **16.87 ms** (60 fps, vsync) | 16.66 | 17.03 | 50.44 (single hitch) |
| GPU | **12.39 ms** | 12.22 | 14.10 | 15.79 |
| Game thread | 3.48 ms | 3.45 | 4.05 | 5.63 |
| RHI thread | 1.42 ms | 1.41 | 1.60 | 1.85 |

- **GPU breakdown:**

  | Pass | Time |
  |---|---|
  | Lumen screen probe gather | 5.09 ms |
  | Unaccounted | 2.59 ms |
  | Lumen scene lighting | 1.13 ms |
  | Shadow depths (VSM) | 0.75 ms |
  | Deferred lighting | 0.47 ms |
  | Shadow projection | 0.32 ms |
  | Post-processing | 0.25 ms |
  | Base pass | 0.25 ms |
  | Lumen reflections | 0.24 ms |
  | **Nanite visibility buffer plus base pass** | **0.35 ms** |

  The model's triangle count is nearly free.
- **Texture streaming:** 236 MB wanted, 100% resident. Non-streaming mips are 626 MB, mostly the runtime Cesium tile textures.

**Reading:** a stable 60 fps at 1080p, with about 4 ms of GPU headroom. At 4K (a demo TV), keep TSR at a 50–67% screen percentage rather than rendering native. On a weaker demo machine, drop the Lumen quality settings in `PPV_LFF_Cinematic` from 2 to 1 first, then use scalability "High". Re-measure on the actual demo hardware (TEST_PLAN T3.6).

**Still to measure:** the other three camera targets, reduced-motion fades, the packaged Development build, and Win64.
