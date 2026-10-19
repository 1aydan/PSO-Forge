# Changelog

All notable changes to PSOForge are recorded here. Versions follow the `VersionName` field in
`PSOForge.uplugin`.

## 0.2.2

### Fixed

- **The warmup widget completed on its first tick in projects using the engine's default PSO
  settings.** With `r.PSOPrecaching=1`, `r.ShaderPipelineCache.UsePrecachingThreadPool` (default
  `1`) makes the shader pipeline cache ignore `StartupMode` and `SetBatchMode`. The cache hands
  bundled PSOs to `PipelineStateCache::PrecacheGraphicsPipelineState` during engine init, in batches
  of `r.ShaderPipelineCache.PSOPrecacheThreadPoolBatchSize` (500), and counts each one complete on
  hand-off (`ShaderPipelineCache.cpp:1212`, `:1342`). A 4,000-PSO cache reported done 0.55s after
  opening, seconds before the boot map loaded. From then on, the outstanding compiles exist only in
  `PipelineStateCache::NumActivePrecacheRequests()`, and 0.2.1 subtracted exactly that count, so
  the widget always read 0 remaining. In this mode the widget now counts those requests. Outside it,
  the 0.2.1 behaviour is unchanged.
- In that mode, the widget logs a one-time warning when it starts after the bundled cache has been
  handed off, pointing at the cvars that make the cache wait for it.
- `RunUAT BuildPlugin` failed in two places, both hidden when building inside a project. UHT
  requires an explicit `Category` on the `BindWidgetOptional` widget properties for engine-installed
  plugins, and `PSOForgeShowcase.cpp` used `EMaterialDomain` without including `MaterialDomain.h`,
  which only compiled when unity builds pulled the header in.

### Changed

- README: documented `r.ShaderPipelineCache.UsePrecachingThreadPool=0` +
  `r.ShaderPipelineCache.StartupMode=0` for warmup screens that should cover the whole bundled
  cache, and the trade-off of starting the cache paused.

## 0.2.1

### Added

- `MaxInProgressFraction` warmup setting (default `0.99`), a ceiling on reported progress while the
  pass is still running so the bar cannot read full before it is. Set to `1.0` to disable.

### Fixed

- **The warmup widget sat on a full progress bar until its timeout fired.** Completion was gated on
  `FShaderPipelineCache::NumPrecompilesRemaining() <= 0`, which that API never returns while work is
  outstanding — it ends in `FMath::Max(PendingCaches, NumRemaining)` with `PendingCaches` set to 1
  for as long as `IsPrecompiling()` is true (`ShaderPipelineCache.cpp:888`). The gate was therefore
  unsatisfiable during the pass, and the only exit was `MaxCompilationTimeSeconds` (60s by default).
  Meanwhile `1 - 1/InitialPrecompileCount` drove the bar to visually full, so the screen looked
  finished and stayed put. Completion now also checks `IsPrecompiling()` directly.
- `NumPrecompilesRemaining()` adds `PipelineStateCache::NumActivePrecacheRequests()` on top of the
  bundled-cache tasks. Those runtime requests keep arriving while the loading screen renders and were
  holding the drain gate open long after the bundled cache had finished, so they are now subtracted
  back out. The widget no longer blocks on runtime PSO precaching.
- The progress denominator was sampled in the same block as `ResumeBatching()`, before the render
  thread had opened the cache. The count reads 0 there, which pinned `InitialPrecompileCount` to 1
  and set progress straight to `1.0`. It is now a high-water mark maintained per tick, and progress
  is ratcheted so raising the baseline makes the bar plateau rather than run backwards.
- The elapsed-time readout always rendered `00:00:00`. `FText::AsTimespan` takes an `FTimespan`,
  whose `int64` constructor is implicit and counts *ticks*, so passing seconds through it silently
  produced a near-zero span. Now converted with `FTimespan::FromSeconds`.

## 0.2.0

### Added

- Unreal Engine 5.8 support. `main` builds against 5.7 and 5.8 from the same source; the `.uplugin`
  carries no `EngineVersion`, so it stays version-agnostic.
- `MinShadowQuality` capture setting (default `1`), a floor applied to the shadow group of each
  swept preset. See the fix below for why it exists.
- Supported-engine table in the README.

### Fixed

- **Capture runs aborted with an engine assert on the low preset.** Sweeping to sg level 0 sets
  `r.ShadowQuality=0`. That cvar is `ECVF_RenderThreadSafe`, so its non-game-thread copy is written
  by an enqueued render command, while `FSceneRenderer::CreateDynamicShadows` (which gates shadow
  setup on `GetShadowQuality() > 0`) and the shadow projection pass both read that copy from async
  worker tasks. A write landing between those two reads leaves a frame that built shadows at one
  quality and projects them at another; at 0 that reaches the unclamped `1..5` switch in
  `FProjectedShadowInfo::RenderOnePassPointLightProjection` and trips `check(0)`
  (`ShadowRendering.cpp:1801` in 5.8, `:1689` in 5.7 — the directional path clamps here, the
  point-light path never has). The environment rig's shadow-casting point light hit this and took
  the process down with exit code 3, leaving no recordings.

  Two changes: rendering is now drained *before* each `Scalability::SetQualityLevels` call, so no
  frame is in flight when the write happens; and `MinShadowQuality` keeps the swept shadow group off
  0 entirely. Shadow quality 0 draws no shadow projections and so contributes no PSOs, which is why
  the floor costs nothing.
- `UMaterial::bUsedWithSkeletalMesh` / `bUsedWithInstancedStaticMeshes` are deprecated in 5.8
  (`UE_DEPRECATED(5.8, "Always use the GetUsageByFlag() accessor.")`). The showcase now calls
  `GetUsageByFlag(MATUSAGE_SkeletalMesh)` / `GetUsageByFlag(MATUSAGE_InstancedStaticMeshes)`, which
  is public on `UMaterial` in 5.7 as well, so one source builds warning-free on both.

## 0.1.0

Initial release.

- `PSOForge` capture module: batched sweep of every project material and Niagara system across all
  scalability presets under `-logPSO`, on static, plane, skeletal, and instanced geometry plus
  decal, post-process, and UI paths. Development-only via `TargetConfigurationAllowList`, with a
  `#error` guard against misconfiguration.
- `PSOForgeRuntime` module: `UPSOForgeWarmupWidget`, a shipping-safe UMG base class for boot and
  loading screens that switches the bundled cache to fast batching, reports precompile progress and
  elapsed time, and restores background batching when done.
- `PSOForgeEditor` module: **Tools → PSOForge Cache Builder**, converting recordings into a `.spc`
  bundle from a dev-managed folder list without touching the command line.
- `Tools/Invoke-PSOForge.ps1`: headless cook → capture → convert → deploy, with support for
  source-build engines and multi-target projects.
- Console commands `PSOForge.Start` / `PSOForge.Stop` / `PSOForge.Status`, the `-PSOForge`
  auto-start switch, and a viewport progress overlay that warns when `-logPSO` is absent.
