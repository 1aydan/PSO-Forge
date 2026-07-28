# PSOForge

Developer-side PSO capture for Unreal Engine 5. PSOForge automates the tedious part of the
[bundled PSO cache workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/optimizing-rendering-with-pso-caches-in-unreal-engine):
it cycles **every scalability preset** in a cooked build while `-logPSO` recording is active, and
**actually renders** every material and Niagara system in your project on representative geometry so
their draw PSOs are created and recorded — then converts the recordings into the `.spc` bundle your
packaged game precompiles during load screens.

The plugin has two modules:

- **PSOForge** — the capture tooling. Development only: `TargetConfigurationAllowList` in the
  `.uplugin` keeps it out of Shipping builds entirely (a `#error` guard makes any misconfiguration
  fail the build loudly).
- **PSOForgeRuntime** — shipping-safe player-facing pieces, currently `UPSOForgeWarmupWidget`: a
  UMG base class for the boot/loading screen that switches the bundled cache to fast batching,
  shows precompile progress, and restores background batching when done.

## Why

- **PSO precaching** (`r.PSOPrecaching`, UE 5.2+) covers most asset PSOs at runtime, but not global
  shaders, compute, and assorted gaps.
- **Bundled PSO caches** cover the rest, but collecting them means playing the whole game under every
  quality preset per release — that's the part PSOForge automates.
- Simply *loading* a material creates no graphics PSOs; the material must be **drawn** on the vertex
  factories it ships on. PSOForge renders each material on static, plane, skeletal (when flagged),
  and instanced meshes, plus decal, post-process, and UI paths, and simulates Niagara systems.

## Supported engine versions

| Engine | Status |
|---|---|
| UE 5.8 | Supported |
| UE 5.7 | Supported |
| UE 5.6 and earlier | Untested |

`main` builds against both 5.7 and 5.8 from the same source — the plugin ships no `EngineVersion`
in its `.uplugin`, so it stays version-agnostic. If a future change forces 5.8-only APIs, a
`release/5.7` branch will be cut from the last commit that still builds on 5.7.

## Setup

1. Drop `PSOForge/` into your project's `Plugins/` folder and enable it in the `.uproject`.
2. `Config/DefaultEngine.ini`:

   ```ini
   [DevOptions.Shaders]
   NeedsShaderStableKeys=true

   [SystemSettings]
   r.ShaderPipelineCache.Enabled=1
   r.ShaderPipelineCache.ExcludePrecachePSO=1
   r.PSOPrecaching=1
   r.PSOPrecache.ProxyCreationWhenPSOReady=1
   ```

3. `Config/DefaultGame.ini` (packaging settings must share material shader code):

   ```ini
   [/Script/UnrealEd.ProjectPackagingSettings]
   bShareMaterialShaderCode=True
   bSharedMaterialNativeLibraries=True
   ```

4. Optional but recommended — Project Settings → Plugins → PSOForge:
   - `ShowcaseSkeletalMesh`: a small character mesh so skinned-material PSOs get captured.
   - `ExcludePathPatterns`: content roots to skip (developer folders, cinematics-only, etc.).
   - `CaptureMap`: a dedicated capture level, if you prefer one over the auto-spawned
     environment rig (lights/fog/floor/camera spawned into whatever map is loaded).

## Usage

One command, end to end (cook → capture → convert → deploy):

```powershell
.\Plugins\PSOForge\Tools\Invoke-PSOForge.ps1 -ProjectFile C:\Path\To\YourGame.uproject
```

Reuse an existing staged build with `-SkipCook`. See the script header for all parameters
(`-Configuration`, `-ShaderFormat`, `-StagedDir`, `-KeepRecordings`, `-NoBundleCopy`, ...).

The resulting `Build/Windows/PipelineCaches/<Project>_<ShaderFormat>.spc` is picked up automatically
by the next `BuildCookRun` package.

Manual/console usage inside any cooked build launched with `-logPSO`:

| Command | Effect |
|---|---|
| `PSOForge.Start` | Start a capture pass in the current world |
| `PSOForge.Stop` | Cancel the running pass |
| `PSOForge.Status` | Log state, batch/preset progress, PSOs logged |

Launching the game with `-PSOForge` auto-starts the capture after the first map loads and quits the
process when done (this is what the script uses).

### Bonus: warming the editor

Running `PSOForge.Start` in a **PIE session** doubles as an editor shader warmer: no PSOs are
recorded (the bundled-cache pipeline only exists in cooked builds — the overlay will say
"recording OFF"), but the sweep force-compiles every material into the DDC and creates their
driver-side PSOs, which eliminates editor compile/first-draw hitches on that machine until shaders
are invalidated. Note the preset sweep only adds editor value for materials using Quality Switch
nodes; for a quick warmup, temporarily trim `QualityPresets` to your working preset in Project
Settings. For team-wide editor warmth, prefer a shared DDC (+ a nightly
`DerivedDataCacheCommandlet` fill) — this command warms the local machine only.

### Capture details

- Batch-outer / preset-inner loop: each material batch is loaded once, spawned once, then rendered
  for `WarmupFramesPerPreset` frames under every `QualityPresets` entry before the next batch —
  every quality tier produces different PSO permutations.
- The orchestration script runs the capture with `r.PSOPrecaching=0` and
  `r.ShaderPipelineCache.ExcludePrecachePSO=0` so **everything drawn is recorded**; shipped builds
  keep precaching on and the two systems cooperate at runtime.
- A viewport overlay shows live progress, including a red **recording OFF** warning when the process
  wasn't launched with `-logPSO` (e.g. in-editor bring-up runs).

## Runtime warmup screen

Subclass `UPSOForgeWarmupWidget` (module `PSOForgeRuntime`) in UMG for your boot or loading screen:

- Name a ProgressBar `PB_ShaderProgress` and/or a TextBlock `TXT_ShaderProgress` to auto-wire them,
  or bind the `OnShaderProgressChanged` / `OnShaderCompileCompleted` events for custom visuals.
- `bAutoStartCompilation` starts the pass on construct; `NextLevelName` (with
  `bOpenLevelOnComplete`) travels onward when precompiles finish.
- `MinDisplaySeconds` avoids a one-frame flash when nothing is pending; `MaxCompilationTimeSeconds`
  is a safety timeout so a bad cache state can never trap the player.

## In-editor cache builder

**Tools → PSOForge Cache Builder** (module `PSOForgeEditor`) converts recordings without touching
the command line:

- Recordings come from a dev-managed folder list (persisted per project): add any folder — build
  roots are scanned recursively for `.rec.upipelinecache`, so cooked builds can live anywhere.
  `.shk` stable keys are picked up from the last cook automatically.
- Pick the shader format, press **Build Cache** — the `ShaderPipelineCacheTools expand` commandlet
  runs as a background process with its output streamed into the panel, so the editor stays
  responsive.
- With "Deploy" checked the resulting `.spc` lands in `Build/Windows/PipelineCaches/` ready for the
  next package; unchecked it stays in `Saved/PSOForge/`.

The PowerShell script remains the headless/CI path; both produce identical bundles.

## Verifying the bundle

1. Package with the `.spc` present; the build log should open the pipeline cache and drain
   precompiles at boot (`LogRHI` / `FShaderPipelineCache` lines).
2. A/B test hitching with `-clearPSODriverCache` on builds with and without the bundle
   (`stat PSO`, `stat unit`).
3. Run a dev build with `r.PSOPrecache.Validation=2` and watch for precache-miss warnings.

## Known limitations

- Volume and light-function material domains are not exercised yet.
- Landscape/water/hair vertex factories are not covered by the showcase; capture those with a
  scripted flythrough of a real map (recordings accumulate across runs).
- Windows/DX12 (`PCD3D_SM6`) is the tested path; other shader formats via `-ShaderFormat`.

## License

MIT — see [LICENSE](LICENSE).
