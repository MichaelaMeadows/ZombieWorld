param(
    [ValidateSet('template_debug','template_release')][string]$Target = 'template_debug',
    [switch]$Bootstrap,
    [switch]$Tests,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$bindings = Join-Path $workspace 'native/vendor/godot-cpp'
$pin = 'e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77'
$pythonDeps = Join-Path $PSScriptRoot '.python-deps'
if ($Bootstrap) {
    if (-not (Test-Path -LiteralPath $bindings)) {
        & git clone --depth 1 --branch godot-4.5-stable https://github.com/godotengine/godot-cpp.git $bindings
        if ($LASTEXITCODE -ne 0) { throw 'Failed to obtain godot-cpp.' }
    }
    & $Python -m pip install --target $pythonDeps scons==4.9.1
    if ($LASTEXITCODE -ne 0) { throw 'Failed to obtain SCons.' }
}
if (-not (Test-Path -LiteralPath $bindings)) { throw 'Run tools/build_campaign.ps1 -Bootstrap first.' }
$revision = & git -C $bindings rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $revision -ne $pin) { throw 'Unexpected godot-cpp revision; use the pinned 4.5 bindings.' }
$previousPythonPath = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $pythonDeps
    Push-Location (Join-Path $workspace 'native/campaign')
    try {
        $argsList = @('-m','SCons','platform=windows','arch=x86_64',"target=$Target",
            'build_profile=build_profile.json','disable_exceptions=no','-j8')
        if ($Tests) { $argsList += 'build_tests=yes' }
        & $Python @argsList
        if ($LASTEXITCODE -ne 0) { throw 'Campaign native build failed.' }
        if ($Tests) {
            New-Item -ItemType Directory -Force -Path 'build/Artifacts/World' | Out-Null
            Push-Location build
            try {
                & .\campaign_tests.exe --test
                if ($LASTEXITCODE -ne 0) { throw 'Portable campaign checks failed.' }
            } finally { Pop-Location }
        }
    } finally { Pop-Location }
} finally { $env:PYTHONPATH = $previousPythonPath }
