param(
    [string]$OnnxRuntimeVersion = "1.23.1",
    [string]$KokoroRepo = "onnx-community/Kokoro-82M-v1.0-ONNX"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

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

$thirdPartyDir = Join-Path $repoRoot "third_party"
$runtimeRoot = Join-Path $thirdPartyDir "onnxruntime"
$nupkgPath = Join-Path $thirdPartyDir "onnxruntime.nupkg"
$zipPath = Join-Path $thirdPartyDir "onnxruntime.zip"
$assetsDir = Join-Path $repoRoot "assets"
$onnxDir = Join-Path $assetsDir "onnx"
$modelPath = Join-Path $onnxDir "model.onnx"
$tokenizerPath = Join-Path $assetsDir "tokenizer.json"
$defaultVoicePath = Join-Path $assetsDir "voices\af_bella.bin"

Ensure-Directory $thirdPartyDir
Ensure-Directory $onnxDir

$runtimeHeader = Join-Path $runtimeRoot "build\native\include\onnxruntime_cxx_api.h"
$runtimeLib = Join-Path $runtimeRoot "runtimes\win-x64\native\onnxruntime.lib"

if (!(Test-Path $runtimeHeader) -or !(Test-Path $runtimeLib)) {
    $runtimeUrl = "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/$OnnxRuntimeVersion/microsoft.ml.onnxruntime.$OnnxRuntimeVersion.nupkg"
    if (!(Test-Path $nupkgPath)) {
        Invoke-WebRequest -Uri $runtimeUrl -OutFile $nupkgPath
    }

    Copy-Item $nupkgPath $zipPath -Force
    Expand-Archive -Path $zipPath -DestinationPath $runtimeRoot -Force
}

Assert-Path $runtimeHeader "Missing ONNX Runtime headers under $runtimeRoot"
Assert-Path $runtimeLib "Missing ONNX Runtime import library under $runtimeRoot"

$pythonCommand = Get-Command python -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    throw "python is required to restore model assets"
}

python -c "import huggingface_hub" | Out-Null

$assetsDirForPython = $assetsDir -replace "\\", "/"

if (!(Test-Path $modelPath)) {
    python -c "from huggingface_hub import hf_hub_download; print(hf_hub_download('$KokoroRepo', 'onnx/model.onnx', local_dir=r'$assetsDirForPython'))"
}

if (!(Test-Path $tokenizerPath)) {
    python -c "from huggingface_hub import hf_hub_download; print(hf_hub_download('$KokoroRepo', 'tokenizer.json', local_dir=r'$assetsDirForPython'))"
}

Assert-Path $modelPath "Missing $modelPath"
Assert-Path $tokenizerPath "Missing $tokenizerPath"
Assert-Path $defaultVoicePath "Missing default voice asset at $defaultVoicePath"

Write-Host "Restored runtime and model assets under $repoRoot"
