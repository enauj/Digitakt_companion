param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$DSPOnly,
    [switch]$SkipTests,
    [string]$PackageDirectory = 'dist'
)
$ErrorActionPreference = 'Stop'
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

$buildDir = Join-Path $PSScriptRoot $(if ($DSPOnly) { 'build-dsp' } else { 'build' })
$juceDir = Join-Path $PSScriptRoot 'third_party/JUCE'
$clapDir = Join-Path $PSScriptRoot 'third_party/clap-juce-extensions'
$clapRevision = '7adee3a1bd4684d4caa5601100e364abccff4b4d'
if (-not $DSPOnly -and -not (Test-Path -LiteralPath (Join-Path $juceDir 'CMakeLists.txt'))) {
    Invoke-Checked git @('clone', '--depth', '1', '--branch', '8.0.16-security', 'https://github.com/juce-framework/JUCE.git', $juceDir)
}
if (-not $DSPOnly) {
    $revision = & git -C $juceDir rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $revision -ne 'ea77444750de45e036e0279bb0291144bf8e3688') {
        throw 'Unexpected JUCE revision. This build is pinned to 8.0.16-security (ea77444750de45e036e0279bb0291144bf8e3688).'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $clapDir 'CMakeLists.txt'))) {
        Invoke-Checked git @('clone', '--no-checkout', 'https://github.com/free-audio/clap-juce-extensions.git', $clapDir)
        Invoke-Checked git @('-C', $clapDir, 'checkout', '--detach', $clapRevision)
        Invoke-Checked git @('-C', $clapDir, 'submodule', 'update', '--init', '--recursive')
    }
    $actualClapRevision = & git -C $clapDir rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actualClapRevision -ne $clapRevision) { throw 'Unexpected clap-juce-extensions revision.' }
}
$dspOption = if ($DSPOnly) { 'ON' } else { 'OFF' }
Invoke-Checked cmake @('-S', $PSScriptRoot, '-B', $buildDir, '-A', 'x64', "-DCOMPANION_DSP_ONLY=$dspOption")
Invoke-Checked cmake @('--build', $buildDir, '--config', $Configuration, '--parallel', '1')
if (-not $SkipTests) { Invoke-Checked ctest @('--test-dir', $buildDir, '-C', $Configuration, '--output-on-failure') }
if (-not $DSPOnly) {
    $destination = Join-Path $PSScriptRoot $PackageDirectory
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDir "DigitaktCompanion_artefacts/$Configuration/Digitakt Companion.exe") -Destination $destination -Force
    $pluginDestination = Join-Path $destination 'Plugins'
    New-Item -ItemType Directory -Path $pluginDestination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDir "CompanionPlugin_artefacts/$Configuration/VST3/DTX Companion.vst3") -Destination $pluginDestination -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $buildDir "CompanionPlugin_artefacts/$Configuration/CLAP/DTX Companion.clap") -Destination $pluginDestination -Force
    Copy-Item -LiteralPath (Join-Path $clapDir 'LICENSE.md') -Destination (Join-Path $destination 'CLAP-ADAPTER-LICENSE.md') -Force
    Copy-Item -LiteralPath (Join-Path $clapDir 'clap-libs/clap/LICENSE') -Destination (Join-Path $destination 'CLAP-SDK-LICENSE.txt') -Force
    Copy-Item -LiteralPath (Join-Path $clapDir 'clap-libs/clap-helpers/LICENSE') -Destination (Join-Path $destination 'CLAP-HELPERS-LICENSE.txt') -Force
    Copy-Item -LiteralPath (Join-Path $juceDir 'modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt') -Destination (Join-Path $destination 'VST3-SDK-LICENSE.txt') -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'MIDI_CC_REFERENCE.md') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'DSP_RETRIGGER_NOTES.md') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RECORDING.md') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../VERSION.txt') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../FUTURE_IDEAS.md') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD_PARTY.md') -Destination $destination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Patches') -Destination $destination -Recurse -Force
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'VALIDATION.md')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'VALIDATION.md') -Destination $destination -Force
    }
    Copy-Item -LiteralPath (Join-Path $juceDir 'LICENSE.md') -Destination (Join-Path $destination 'JUCE-LICENSE.md') -Force
    Write-Host "App ready: $destination\Digitakt Companion.exe"
}
