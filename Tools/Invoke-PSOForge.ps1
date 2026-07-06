<#
.SYNOPSIS
    End-to-end PSO capture pipeline: cook/stage the game, run the PSOForge capture pass with
    -logPSO, harvest the recorded PSOs, and build the bundled cache (.spc) for packaging.

.DESCRIPTION
    Steps:
      1. (unless -SkipCook) BuildCookRun: build + cook + stage + pak the Win64 client.
      2. Copy the shader stable key (.shk) files produced by the cook.
      3. Launch the staged build with -logPSO -clearPSODriverCache -PSOForge. The PSOForge plugin
         auto-runs the capture (every material x every scalability preset) and quits when done.
         PSO precaching is disabled for the run so everything drawn is recorded.
      4. Harvest the .rec.upipelinecache recordings.
      5. ShaderPipelineCacheTools expand -> <Project>_<ShaderFormat>.spc
      6. Copy the .spc into Build/Windows/PipelineCaches so the next package bundles it.

.EXAMPLE
    .\Invoke-PSOForge.ps1 -ProjectFile F:\Work\MyGame\MyGame.uproject
.EXAMPLE
    .\Invoke-PSOForge.ps1 -ProjectFile F:\Work\MyGame\MyGame.uproject -SkipCook -KeepRecordings
#>
[CmdletBinding()]
param(
    [string]$ProjectFile,
    [string]$EngineDir,
    [switch]$SkipCook,
    [string]$StagedDir,
    [ValidateSet('Development', 'DebugGame', 'Test')]
    [string]$Configuration = 'Development',
    [string]$ShaderFormat = 'PCD3D_SM6',
    [int]$TimeoutMinutes = 120,
    [string]$OutputDir,
    [switch]$KeepRecordings,
    [switch]$NoBundleCopy
)

$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    Write-Host "PSOForge: ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Info([string]$Message) {
    Write-Host "PSOForge: $Message" -ForegroundColor Cyan
}

# ---- Locate project -------------------------------------------------------
if (-not $ProjectFile) {
    # Walk up from the plugin's Tools folder looking for a .uproject (works when the plugin
    # lives under <Project>/Plugins/PSOForge).
    $Probe = $PSScriptRoot
    while ($Probe -and -not $ProjectFile) {
        $Candidate = Get-ChildItem -Path $Probe -Filter '*.uproject' -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($Candidate) { $ProjectFile = $Candidate.FullName }
        $Probe = Split-Path $Probe -Parent
    }
}
if (-not $ProjectFile -or -not (Test-Path $ProjectFile)) {
    Fail 'No .uproject found. Pass -ProjectFile <path to .uproject>.'
}
$ProjectFile = (Resolve-Path $ProjectFile).Path
$ProjectRoot = Split-Path $ProjectFile -Parent
$ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectFile)
if (-not $OutputDir) { $OutputDir = Join-Path $ProjectRoot 'Saved\PSOForge' }
Info "Project: $ProjectName ($ProjectFile)"

# ---- Locate engine --------------------------------------------------------
if (-not $EngineDir) {
    $Association = (Get-Content $ProjectFile -Raw | ConvertFrom-Json).EngineAssociation
    if ($Association) {
        $Reg = Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association" -ErrorAction SilentlyContinue
        if ($Reg) { $EngineDir = $Reg.InstalledDirectory }
        if (-not $EngineDir) {
            $Builds = Get-ItemProperty 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
            if ($Builds -and $Builds.$Association) { $EngineDir = $Builds.$Association }
        }
    }
}
if (-not $EngineDir -or -not (Test-Path $EngineDir)) {
    Fail 'Engine directory not found. Pass -EngineDir <UE install root>.'
}
$RunUAT = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
$EditorCmd = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($Required in @($RunUAT, $EditorCmd)) {
    if (-not (Test-Path $Required)) { Fail "Missing engine file: $Required" }
}
Info "Engine: $EngineDir"

# ---- Cook / stage ----------------------------------------------------------
if (-not $StagedDir) { $StagedDir = Join-Path $ProjectRoot 'Saved\StagedBuilds\Windows' }

if (-not $SkipCook) {
    Info "Cooking and staging ($Configuration)... this can take a while."
    & $RunUAT BuildCookRun "-project=$ProjectFile" -platform=Win64 "-clientconfig=$Configuration" `
        -build -cook -stage -pak -iostore -nop4 -utf8output -unattended
    if ($LASTEXITCODE -ne 0) { Fail "BuildCookRun failed with exit code $LASTEXITCODE." }
}

$GameExe = Join-Path $StagedDir "$ProjectName\Binaries\Win64\$ProjectName.exe"
if (-not (Test-Path $GameExe)) {
    # DebugGame/Test binaries carry a suffix, e.g. MyGame-Win64-DebugGame.exe.
    $GameExe = Get-ChildItem (Join-Path $StagedDir "$ProjectName\Binaries\Win64") -Filter "$ProjectName*.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $GameExe -or -not (Test-Path $GameExe)) {
    Fail "Staged game executable not found under $StagedDir. Cook first or pass -StagedDir."
}
Info "Staged executable: $GameExe"

# ---- Harvest stable shader keys -------------------------------------------
$ShkDir = Join-Path $OutputDir 'shk'
New-Item -ItemType Directory -Force $ShkDir | Out-Null
$CookedMetadata = Join-Path $ProjectRoot "Saved\Cooked\Windows\$ProjectName\Metadata\PipelineCaches"
$ShkFiles = Get-ChildItem $CookedMetadata -Filter "*$ShaderFormat.shk" -ErrorAction SilentlyContinue
if (-not $ShkFiles) {
    Fail "No .shk stable key files under $CookedMetadata. Is NeedsShaderStableKeys=true set in DefaultEngine.ini [DevOptions.Shaders], and did the cook run?"
}
$ShkFiles | Copy-Item -Destination $ShkDir -Force
Info "Stable keys: $($ShkFiles.Name -join ', ')"

# ---- Run capture -----------------------------------------------------------
$GameArgs = @(
    '-logPSO', '-clearPSODriverCache', '-PSOForge',
    '-dpcvars=r.PSOPrecaching=0,r.ShaderPipelineCache.ExcludePrecachePSO=0',
    '-unattended', '-nosplash', '-windowed', '-resx=1280', '-resy=720', '-log'
)
Info "Launching capture: $GameExe $($GameArgs -join ' ')"
$Proc = Start-Process -FilePath $GameExe -ArgumentList $GameArgs -PassThru
if (-not $Proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $Proc.Kill()
    Fail "Capture run exceeded $TimeoutMinutes minutes and was killed."
}
Info "Capture run exited with code $($Proc.ExitCode)."

# Echo the summary line from the game log if present.
$GameLog = Join-Path $StagedDir "$ProjectName\Saved\Logs\$ProjectName.log"
if (Test-Path $GameLog) {
    $Summary = Select-String -Path $GameLog -Pattern 'CaptureComplete' | Select-Object -Last 1
    if ($Summary) { Info $Summary.Line.Trim() } else { Write-Warning 'No CaptureComplete line found in the game log.' }
}

# ---- Harvest recordings ----------------------------------------------------
$RecDir = Join-Path $OutputDir 'rec'
New-Item -ItemType Directory -Force $RecDir | Out-Null
$RecSources = @(
    (Join-Path $StagedDir "$ProjectName\Saved\CollectedPSOs"),
    (Join-Path $env:LOCALAPPDATA "$ProjectName\Saved\CollectedPSOs")
)
$RecFiles = @()
foreach ($Source in $RecSources) {
    if (Test-Path $Source) {
        $RecFiles += Get-ChildItem $Source -Filter '*.rec.upipelinecache' -File
    }
}
$RecFiles = $RecFiles | Where-Object Length -gt 0
if (-not $RecFiles) {
    Fail "No non-empty .rec.upipelinecache recordings found in: $($RecSources -join '; ')"
}
$RecFiles | Copy-Item -Destination $RecDir -Force
Info "Recordings harvested: $($RecFiles.Count) file(s), $([Math]::Round(($RecFiles | Measure-Object Length -Sum).Sum / 1KB)) KB total."

# ---- Expand to bundled cache ----------------------------------------------
$SpcFile = Join-Path $OutputDir "${ProjectName}_$ShaderFormat.spc"
Info 'Running ShaderPipelineCacheTools expand...'
& $EditorCmd $ProjectFile -run=ShaderPipelineCacheTools expand `
    (Join-Path $RecDir '*.rec.upipelinecache') (Join-Path $ShkDir '*.shk') $SpcFile
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $SpcFile)) {
    Fail "ShaderPipelineCacheTools expand failed (exit $LASTEXITCODE)."
}
Info "Bundled cache written: $SpcFile ($([Math]::Round((Get-Item $SpcFile).Length / 1KB)) KB)"

# ---- Deploy ----------------------------------------------------------------
if (-not $NoBundleCopy) {
    $DeployDir = Join-Path $ProjectRoot 'Build\Windows\PipelineCaches'
    New-Item -ItemType Directory -Force $DeployDir | Out-Null
    Copy-Item $SpcFile -Destination $DeployDir -Force
    Info "Deployed to $DeployDir — the next package will bundle it."
}

if (-not $KeepRecordings) {
    Remove-Item $RecDir -Recurse -Force -Confirm:$false
}

Info 'Done.'
