<#
.SYNOPSIS
Pack and publish every ELifeRPG addon to the Workshop through the Workbench CLI.

.DESCRIPTION
Addons are processed in dependency order, derived from addons/*/*.gproj. The run stops at the
first failure, so no addon is published against a dependency that failed.

Workbench always exits with 0, so success is judged from the packed output and from
Workbench's console.log.

Machine-specific paths come from the environment:
  REFORGER_WORKBENCH  full path to ArmaReforgerWorkbenchSteamDiag.exe
  REFORGER_ADDONS     vanilla addons directory of the Arma Reforger Tools / game install

Requires a logged-in Steam session for the current Windows user (Workbench takes its Bohemia
session from it), and each addon must have been published once through the Workbench GUI.

.EXAMPLE
pwsh tools/publish.ps1 -PackOnly
pwsh tools/publish.ps1 -ChangeNoteFile notes.txt
#>
param(
    [string]$Version,
    [string]$ChangeNoteFile,
    [switch]$PackOnly,
    [int]$TimeoutMinutes = 30
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$addonsRoot = Join-Path $repo 'addons'
$outRoot = Join-Path $repo 'out'

if (-not $Version) { $Version = (Get-Content (Join-Path $repo 'VERSION') -Raw).Trim() }
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid version '$Version' (expected X.Y.Z)" }

# --- Dependency order ---------------------------------------------------------------------
$addons = @{}
foreach ($gproj in Get-ChildItem $addonsRoot -Filter *.gproj -Recurse -Depth 1) {
    $text = Get-Content $gproj.FullName -Raw
    $guid = [regex]::Match($text, '(?m)^ GUID "([0-9A-F]{16})"').Groups[1].Value
    $depBlock = [regex]::Match($text, '(?s)Dependencies \{(.*?)\}').Groups[1].Value
    $deps = [regex]::Matches($depBlock, '"([0-9A-F]{16})"') | ForEach-Object { $_.Groups[1].Value }
    $name = $gproj.Directory.Name
    if ($name -like 'ELifeRPG_Patches_*') { continue }  # depend on third-party mods; published separately
    $addons[$guid] = [pscustomobject]@{ Name = $name; Gproj = $gproj.FullName; Deps = @($deps) }
}

$order = [System.Collections.Generic.List[object]]::new()
$state = @{}  # guid -> 'visiting' | 'done'
function Visit([string]$guid) {
    if (-not $addons.ContainsKey($guid)) { return }  # vanilla or third-party dependency
    if ($state[$guid] -eq 'done') { return }
    if ($state[$guid] -eq 'visiting') { throw "Dependency cycle at $($addons[$guid].Name)" }
    $state[$guid] = 'visiting'
    foreach ($d in $addons[$guid].Deps) { Visit $d }
    $state[$guid] = 'done'
    $order.Add($addons[$guid])
}
foreach ($g in ($addons.Keys | Sort-Object { $addons[$_].Name })) { Visit $g }

Write-Host "Version $Version, order: $(($order | ForEach-Object Name) -join ' -> ')"

# --- Environment --------------------------------------------------------------------------
$wb = $env:REFORGER_WORKBENCH
if (-not $wb -or -not (Test-Path $wb)) { throw "REFORGER_WORKBENCH not set or not found: '$wb'" }
if (-not $env:REFORGER_ADDONS -or -not (Test-Path $env:REFORGER_ADDONS)) {
    throw "REFORGER_ADDONS not set or not found: '$($env:REFORGER_ADDONS)'"
}
if (-not $PackOnly) {
    if (-not $ChangeNoteFile) {
        $ChangeNoteFile = Join-Path ([IO.Path]::GetTempPath()) "eliferpg-changenote-$Version.txt"
        Set-Content $ChangeNoteFile "Release v$Version"
    }
    $ChangeNoteFile = (Resolve-Path $ChangeNoteFile).Path
}
if (Get-Process ArmaReforgerSteam -ErrorAction SilentlyContinue) {
    throw 'Arma Reforger is running; Workbench hangs while the game holds the Steam session.'
}

$documents = [Environment]::GetFolderPath('MyDocuments')
if (-not $documents) { $documents = $HOME }
$logsRoot = Join-Path $documents 'My Games\ArmaReforgerWorkbench\logs'
$fatalPatterns = @(
    "should have required property 'summary'",  # item never published: do the first publish in the GUI
    'not linked with Bohemia Account',
    'Invalid login credentials',
    'Backend is not enabled'
)

function Invoke-Workbench([string]$label, [string[]]$wbArgs) {
    $started = Get-Date
    $proc = Start-Process -FilePath $wb -ArgumentList $wbArgs -PassThru -NoNewWindow
    if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
        $proc.Kill($true)
        throw "$label`: Workbench did not finish within $TimeoutMinutes minutes"
    }
    $log = Get-ChildItem $logsRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object LastWriteTime -ge $started.AddSeconds(-5) |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 |
        ForEach-Object { Join-Path $_.FullName 'console.log' }
    if (-not $log -or -not (Test-Path $log)) { throw "$label`: no Workbench console.log found under $logsRoot" }
    $logCopy = Join-Path $outRoot 'logs'
    New-Item -ItemType Directory -Force $logCopy | Out-Null
    Copy-Item $log (Join-Path $logCopy (($label -replace '\W', '_') + '.log'))
    $errors = @(
        Select-String -Path $log -SimpleMatch -Pattern $fatalPatterns
        Select-String -Path $log -Pattern '(WorkshopApi|BACKEND).*\(E\)|\(E\).*(WorkshopApi|BACKEND)'
    ) | Sort-Object LineNumber -Unique
    if ($errors.Count -gt 0) {
        $errors | ForEach-Object { Write-Host "  $($_.Line)" }
        throw "$label`: Workbench reported errors (log: $log)"
    }
    return $log
}

# --- Pack and publish ---------------------------------------------------------------------
if (Test-Path $outRoot) { Remove-Item $outRoot -Recurse -Force }
$addonsDir = "$($env:REFORGER_ADDONS),$addonsRoot"

foreach ($a in $order) {
    $out = Join-Path $outRoot $a.Name
    $base = @('-wbModule=ResourceManager', '-addonsDir', "`"$addonsDir`"", '-gproj', "`"$($a.Gproj)`"", '-wbSilent', '-noThrow')

    Write-Host "== $($a.Name): pack"
    Invoke-Workbench "$($a.Name) pack" ($base + @('-packAddon', '-packAddonDir', "`"$out`"")) | Out-Null
    if (-not (Get-ChildItem $out -Filter *.pak -Recurse -ErrorAction SilentlyContinue)) {
        throw "$($a.Name): packing produced no .pak in $out"
    }

    if ($PackOnly) { continue }
    Write-Host "== $($a.Name): publish $Version"
    Invoke-Workbench "$($a.Name) publish" ($base + @(
        '-publishAddon', '-publishAddonDir', "`"$out`"",
        '-publishAddonVersion', $Version,
        '-publishAddonChangeNoteFile', "`"$ChangeNoteFile`"")) | Out-Null
}

Write-Host "Done: $($order.Count) addons $(if ($PackOnly) { 'packed' } else { "published as $Version" })"
