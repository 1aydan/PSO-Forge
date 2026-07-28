# Changelog

All notable changes to PSOForge are recorded here. Versions follow the `VersionName` field in
`PSOForge.uplugin`.

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
