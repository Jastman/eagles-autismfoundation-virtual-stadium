# Windows build handoff

This folder contains the editable Unreal project and an October 7 Windows
Development package under `Windows`. Rebuild after changing source or assets;
the October 7 ZIP is an older snapshot, not the London feedback build.
The completed London feedback package is under `Saved\Packaged\Windows`.
Use that entire folder rather than the original `Windows` package or ZIP.

## Required on the Windows PC

- Unreal Engine 5.8.3.
- Visual Studio 2022 17.14+ with Game development with C++, the UE-compatible
  MSVC toolchain, and Windows SDK (or another toolchain supported by UE 5.8).
- The Windows version of Cesium for Unreal, preferably 2.29.1 to match the Mac,
  installed into this engine. Keep the enabled engine runtime/content plugins:
  MetaHumanCrowdContent, MetaHumanCrowd, MetaHumanCharacter, HairStrands,
  MassGameplay, and ChaosClothAsset.
- To rebuild or cook fitted MetaHuman hair assets, install **MetaHuman Creator Core Data**
  from the engine's Options in Epic Games Launcher. Wait for the optional-content
  installation to finish before opening Unreal Editor. A completed base-engine
  installation does not mean this optional data is installed.
- Internet access for streamed Google Photorealistic 3D Tiles.

If Launcher reports `LS-0019-IS-0001`, repair the signed prerequisite installer
bundled under `Engine\Extras\Redist\en-us` with administrator approval. In this
UE 5.8.3 installation it is `vc_redist.x64.exe`, not `UEPrereqSetup_x64.exe`.
Installer exit code `3010` means the repair succeeded but Windows must restart.
Do not skip prerequisites or assume that running Launcher as administrator
completes a repair.

MCP, Terminal, editor toolsets, and VibeUE are disabled in the shared project
descriptor so neither workstation requires optional editor tooling to build.
Enable them only for local editor work and do not commit that enablement.
The unused Android File Server plugin is also disabled so editor startup does
not write a workstation-specific token into shared engine configuration.
No Mac binaries, build caches, or original model sources are needed.
Repository docs, tools, source media, and authored Mac packaging resources are
preserved for cross-platform editing. SourceData/Journey is included for the
C++ editor tests. See the README's workstation synchronization instructions.

## Build

1. Copy this entire folder to a local path, such as C:\\Projects\\VirtualStadium.
2. Open Command Prompt in the folder and compile:

   set "UE_ROOT=C:\\Program Files\\Epic Games\\UE_5.8"
   "%UE_ROOT%\\Engine\\Build\\BatchFiles\\Build.bat" VirtualStadiumEditor Win64 Development -Project="%CD%\\VirtualStadium.uproject" -WaitMutex

3. Open VirtualStadium.uproject. The ion token asset is excluded from Git:
   choose the restricted project token in the Cesium panel and save the recreated
   CesiumIonSaaS asset. Confirm the stadium, tour, Free Explore, audio, and crowd.
4. Close Unreal Editor, then run BuildWindows.cmd from Command Prompt.
   If Unreal is installed elsewhere, set UE_ROOT first.
5. Launch the executable under Saved\\Packaged and check the complete experience.
   Transfer the whole packaged Windows folder, not just the executable.

## Fitted MetaHuman hair

The saved stadium now assigns genuine stock MetaHuman hair cards to all 88
authored people and six detailed runtime appearances. Twelve skeletal meshes
under `Content\LFF\People\Hair` fit three styles per sex to the two actual head
variants; each retains five weighted card LODs and its stock textured materials.
The hair uses the face's animated pose leader with an identity relative transform.
No character conversion, hats, or placeholder scalp geometry is required.
Clothed people also use the long-sleeve garment's shipped face-coverage mask for
the separate face mesh's lower neck and shoulders, alongside the fitted body
coverage masks. Their exposed faces, necks, hands, and hair remain visible.

The level holds hard references to the fitted meshes, and their materials reference
the stock Core Data hair atlases. Keep that optional content installed on the
machine performing the cook. End users need only the complete resulting package.
The completed `Saved\Packaged\Windows` package includes these hair assignments
and the face/body garment masks. Fresh Editor reload verified fitting, materials,
LODs, references, and preservation of the existing scene. The final package was
also checked in native standard and ultrawide views, including live resizing.

## London display

The follow-up scene restores 96 of the 97 previously cleared parked cars;
only the immediate reviewed barrier/queue clearance remains empty.
Concourse walkers use floor-validated passing lanes and swept personal spacing;
crossing pedestrians yield instead of moving through one another.
Stadium materials explicitly support Nanite, and fallback geometry retains
all source triangles so stair treads and thin railings survive non-Nanite
render paths. The eagle's feather materials explicitly support Geometry Cache
rendering in cooked builds.
The anthem is mixed in the foreground during the visible flight, with a lower
crowd bed and modest narration attenuation to keep the combined mix below unity.
Step 22 uses a roughly five-metre, 60-degree conversational camera and a
four-second greeting wave with an independently baked friendly facial pose.
The smile animates the assembled Crowd face's lip corners and cheeks directly;
Creator expression-control curves alone do not animate this optimized face.
This conversational character retains the detailed face LOD so the greeting's
facial joints are not discarded by distance-based crowd simplification.
Its baked joint clip bypasses the stock expression-control post-process, which
would otherwise reset the smile to neutral. Other people retain their normal
body-led facial assembly.

The UI adapts to the live viewport on both standard monitors and ultrawide
displays. It uses shortest-side DPI scaling, bounded corner panels, wrapping
timeline entries, and a dock-safe ticket overlay; the wall resolution is not
hard-coded into the application.
The 3D camera preserves its authored 16:9 vertical field of view, revealing
additional scene width on ultrawide instead of cropping heads and room furnishings.

In Explore and at settled Guided Tour steps, hold either mouse button on the
3D view and drag to look around. A short frame-rate-independent filter smooths
uneven mouse samples without changing the 0.28-degree-per-pixel sensitivity.
The Mouse2D axis explicitly uses unit scaling instead of Unreal's inherited
0.07 legacy multiplier, so the camera receives actual pixels rather than
unexpectedly attenuated input.
Releasing the button, losing window focus, or opening a modal stops look
immediately. Drags over UI controls do not move the camera.
Guided Tour look changes orientation only: narration and the current step keep
playing, flight keys stay disabled, and Next/Back restore the next step's authored
framing. Automatic travel, fades, and cinematic follow retain camera control
until they finish. Explore's movement speed and wheel travel remain unchanged.

The target wall is **3584 x 1280**. Launch the rebuilt package at that resolution:

    Saved\Packaged\Windows\VirtualStadium.exe -windowed -ResX=3584 -ResY=1280 -ForceRes

Use `-fullscreen` instead of `-windowed` on the wall's native 3584 x 1280
Windows display. Do not stretch a 1920 x 1080 video output to fit the wall.
Keep Windows output resolution and the wall processor's input configuration
consistent; application color corrections cannot compensate for a miscalibrated
display.

In a Development build, `LFF.Shots 7 size:1920x1080 size:3584x1280 quit`
captures the ticket step and then resizes the same live viewport without resetting
the tour. Screenshots are saved under the packaged application's `Saved\Screenshots`.

The London audio sources are saved under `SourceData\Audio\London`, and the
normalized narrator WAVs are under `SourceData\Audio\VO`. Their imported
SoundWave assets are included under `Content\LFF\Audio`. Narration is normalized
near -16 LUFS with a -1.5 dB true-peak ceiling. Sound off and Calm Mode still
silence the experience, including button clicks. The UI click is an original
synthesized effect; no Epidemic Sound download is included.

The supplied national-anthem recording and crowd recordings must be covered by
the project's exhibition/distribution permissions. User-supplied media is not
a new license grant.

The first build uses Development for troubleshooting. After validating it, use
-clientconfig=Shipping in BuildWindows.cmd for a Shipping package.
The packaged app still needs internet access and a scoped read-only ion token.
Do not distribute it until asset licensing and attribution requirements are met.

## Cesium tiles showing blue / missing patches

Google Photorealistic 3D Tiles requests to `tile.googleapis.com` can fail transiently (seen as `libcurl error 35 / Connection was reset` over IPv6). Cesium marks a failed tile as permanently empty, so the blue sky-atmosphere globe shows through.
`FLFFRetryingAssetAccessor` (`Source/VirtualStadium/Private/Cesium`) is installed at game-module startup and retries failed GET requests and HTTP 408/429/5xx responses up to 6 times with exponential backoff. Look for `LogVirtualStadium: Cesium:` lines in the log.
If holes persist on a venue network, allow `tile.googleapis.com` and `api.cesium.com` through the firewall/proxy and check IPv6 connectivity.

## Loading screen and tile streaming

- On launch, a midnight-green cover (EAF + Five Below logos, white progress bar, percentage, status text) hides the stadium until Cesium reports the 3D tiles loaded (>= 99.5% for 1 s). It usually takes about 10 s and gives up after 45 s. The welcome orbit is held still behind the cover so the tiles can settle.
- Skip it with `-LFFNoLoadingScreen` (use this for `LFF.Shots` screenshot runs).
- To stop blue holes when the camera turns quickly, every tileset is set at runtime (`ULFFTileStreamingSubsystem`) to:
  - keep loading off-screen tiles at a coarser detail (frustum culling off, culled SSE 32);
  - forbid holes;
  - preload ancestors and siblings;
  - allow 32 simultaneous loads and a 1 GB tile cache.

  If the wall PC runs short of memory or frame rate, lower these in `ConfigureTileset`.
