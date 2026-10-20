param([switch]$Rendered, [string]$Godot = $env:GODOT_BIN)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
if (-not $Godot) { throw 'Set GODOT_BIN or pass -Godot with the Godot console executable.' }
$log = Join-Path $workspace 'docs/verification/campaign-tests-runtime.log'
$engineArgs = @('--path', $workspace, 'res://tests/campaign/campaign_tests.tscn', '--resolution', '1280x720')
if (-not $Rendered) { $engineArgs = @('--headless') + $engineArgs }
& $Godot @engineArgs *> $log
if ($LASTEXITCODE -ne 0) { Get-Content $log -Tail 40; throw 'Campaign runtime tests failed.' }
$text = Get-Content -LiteralPath $log -Raw
if ($text -match 'SCRIPT ERROR|Parse Error|FAIL:' -or $text -notmatch 'CAMPAIGN_RESULT: \d+ assertions, 0 failures') {
    Get-Content $log -Tail 40
    throw 'Campaign runtime checks did not finish cleanly.'
}
$unexpected = ($text -split "`n") | Where-Object { $_ -match '^ERROR:' -and $_ -notmatch '^ERROR: 1 resources still in use at exit' }
if ($unexpected) { throw "Unexpected engine errors: $unexpected" }
Select-String -LiteralPath $log -Pattern 'CAMPAIGN_RESULT'
