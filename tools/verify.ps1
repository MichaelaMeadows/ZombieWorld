param([switch]$Rendered)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$engine = $env:GODOT_BIN
if (-not $engine) {
    $engine = 'C:\Users\micha\Downloads\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64_console.exe'
}
function Invoke-Check([string]$Name, [string[]]$EngineArguments, [string]$Marker = '') {
    $log = Join-Path $workspace "docs\verification\$Name.log"
    & $engine --path $workspace @EngineArguments *> $log
    if ($LASTEXITCODE -ne 0) { throw "$Name failed. See $log" }
    $text = Get-Content -LiteralPath $log -Raw
    if ($text -match 'SCRIPT ERROR|Parse Error|FAIL:') { throw "$Name reported script/test errors. See $log" }
    $unexpectedErrors = ($text -split "`n") | Where-Object { $_ -match '^ERROR:' -and $_ -notmatch '^ERROR: 1 resources still in use at exit' }
    if ($unexpectedErrors) { throw "$Name reported errors: $unexpectedErrors" }
    if ($Marker -and -not $text.Contains($Marker)) { throw "$Name did not finish: expected $Marker" }
    Write-Output "PASS $Name"
}
Invoke-Check 'milestone2-import' @('--headless','--editor','--import','--quit')
Invoke-Check 'combat-tests' @('--headless','res://tests/run_tests.tscn','--quit-after','600') '0 failures'
Invoke-Check 'combat-responsiveness-tests' @('--headless','res://tests/combat_responsiveness_tests.tscn','--fixed-fps','60','--quit-after','600') 'RESPONSIVENESS_RESULT:'
Invoke-Check 'movement-tests' @('--headless','res://tests/movement_direction_tests.tscn','--fixed-fps','60','--quit-after','600') '0 failures'
Invoke-Check 'player-locomotion-tests' @('--headless','res://tests/player_locomotion_tests.tscn','--fixed-fps','60','--quit-after','600') 'PLAYER_LOCOMOTION_RESULT:'
Invoke-Check 'dodge-stamina-tests' @('--headless','res://tests/dodge_stamina_tests.tscn','--quit-after','600') 'DODGE_STAMINA_RESULT:'
Invoke-Check 'greatsword-swing-tests' @('--headless','res://tests/greatsword_swing_tests.tscn','--quit-after','600') 'GREATSWORD_SWING_RESULT:'
Invoke-Check 'bandit-tests' @('--headless','res://tests/bandit_tests.tscn','--fixed-fps','60','--quit-after','600') '0 failures'
Invoke-Check 'modular-tests' @('--headless','res://tests/modular_sprite_tests.tscn','--quit-after','600') '0 failures'
Invoke-Check 'modular-assets-tests' @('--headless','res://tests/modular_asset_tests.tscn','--quit-after','600') '0 failures'
Invoke-Check 'zombie-tests-runtime' @('--headless','res://tests/zombie_tests.tscn','--fixed-fps','60','--quit-after','600') 'ZOMBIE_RESULT: 59 assertions, 0 failures'
Invoke-Check 'encounter-headless' @('--headless','res://tests/encounter_smoke.tscn','--fixed-fps','60','--quit-after','900') 'ENCOUNTER_REPORT:'
Invoke-Check 'layout-headless' @('--headless','res://tests/layout_smoke.tscn','--quit-after','120') 'LAYOUT_RESULT: 0 errors'
if ($Rendered) {
    Invoke-Check 'layout-720' @('res://tests/layout_smoke.tscn','--resolution','1280x720','--quit-after','120') 'LAYOUT_RESULT: 0 errors'
    Invoke-Check 'layout-1080' @('res://tests/layout_smoke.tscn','--resolution','1920x1080','--quit-after','120') 'LAYOUT_RESULT: 0 errors'
}
Write-Output 'Verified. Known upstream state.gd shutdown resource warning remains allowlisted.'
