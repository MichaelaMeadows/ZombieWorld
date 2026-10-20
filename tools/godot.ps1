param([Parameter(ValueFromRemainingArguments=$true)][string[]]$GodotArgs)
$ErrorActionPreference = 'Stop'
$engine = $env:GODOT_BIN
if (-not $engine) {
    $engine = 'C:\Users\micha\Downloads\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64\Godot_v4.7.2-stable_mono_win64_console.exe'
}
if (-not (Test-Path -LiteralPath $engine)) { throw 'Set GODOT_BIN to your Godot 4.7.2 executable.' }
& $engine --path (Split-Path $PSScriptRoot -Parent) @GodotArgs
exit $LASTEXITCODE
