param(
    [string]$Version = "",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$outputRoot = Join-Path $repoRoot "dist"
$bundleBaseName = "Kokoro.C-win-x64"
$bundleName = if ([string]::IsNullOrWhiteSpace($Version)) { $bundleBaseName } else { "$bundleBaseName-$Version" }
$stagingDir = Join-Path $outputRoot $bundleName
$zipPath = Join-Path $outputRoot ("$bundleName.zip")
$buildBinDir = Join-Path $repoRoot ("build\bin\$Configuration")

function Ensure-Directory {
    param([string]$Path)

    if (!(Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Assert-Path {
    param(
        [string]$Path,
        [string]$Message
    )

    if (!(Test-Path $Path)) {
        throw $Message
    }
}

Ensure-Directory $outputRoot

if (Test-Path $stagingDir) {
    Remove-Item $stagingDir -Recurse -Force
}

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Ensure-Directory $stagingDir
Ensure-Directory (Join-Path $stagingDir "assets")
Ensure-Directory (Join-Path $stagingDir "assets\onnx")
Ensure-Directory (Join-Path $stagingDir "assets\voices")

$filesToCopy = @(
    @{ Source = Join-Path $buildBinDir "kokoro_cli.exe"; Target = Join-Path $stagingDir "kokoro_cli.exe"; Message = "Missing built CLI executable" },
    @{ Source = Join-Path $buildBinDir "kokoro_cpp_demo.exe"; Target = Join-Path $stagingDir "kokoro_cpp_demo.exe"; Message = "Missing built demo executable" },
    @{ Source = Join-Path $buildBinDir "onnxruntime.dll"; Target = Join-Path $stagingDir "onnxruntime.dll"; Message = "Missing ONNX Runtime DLL" },
    @{ Source = Join-Path $buildBinDir "onnxruntime_providers_shared.dll"; Target = Join-Path $stagingDir "onnxruntime_providers_shared.dll"; Message = "Missing ONNX Runtime providers DLL" },
    @{ Source = Join-Path $repoRoot "assets\onnx\model.onnx"; Target = Join-Path $stagingDir "assets\onnx\model.onnx"; Message = "Missing default model asset" },
    @{ Source = Join-Path $repoRoot "assets\tokenizer.json"; Target = Join-Path $stagingDir "assets\tokenizer.json"; Message = "Missing tokenizer asset" },
    @{ Source = Join-Path $repoRoot "README.md"; Target = Join-Path $stagingDir "README.md"; Message = "Missing README.md" },
    @{ Source = Join-Path $repoRoot "LICENSE"; Target = Join-Path $stagingDir "LICENSE"; Message = "Missing LICENSE" },
    @{ Source = Join-Path $repoRoot "NOTICE"; Target = Join-Path $stagingDir "NOTICE"; Message = "Missing NOTICE" },
    @{ Source = Join-Path $repoRoot "THIRD_PARTY.md"; Target = Join-Path $stagingDir "THIRD_PARTY.md"; Message = "Missing THIRD_PARTY.md" },
    @{ Source = Join-Path $repoRoot "CHANGELOG.md"; Target = Join-Path $stagingDir "CHANGELOG.md"; Message = "Missing CHANGELOG.md" }
)

foreach ($entry in $filesToCopy) {
    Assert-Path $entry.Source $entry.Message
    Copy-Item $entry.Source $entry.Target -Force
}

$voiceFiles = Get-ChildItem (Join-Path $repoRoot "assets\voices") -Filter *.bin
if ($voiceFiles.Count -eq 0) {
    throw "Missing voice assets under assets/voices"
}

foreach ($voiceFile in $voiceFiles) {
    Copy-Item $voiceFile.FullName (Join-Path $stagingDir "assets\voices\$($voiceFile.Name)") -Force
}

Compress-Archive -Path $stagingDir -DestinationPath $zipPath -Force

Write-Host "Created release archive: $zipPath"
