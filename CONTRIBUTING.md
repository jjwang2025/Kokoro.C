# Contributing

Thanks for contributing to `Kokoro.C`.

## Scope

This project is currently focused on:

- C++17 runtime code
- Windows x64 builds
- ONNX Runtime integration
- Repository-local voice assets
- Local packaging scripts for standalone release bundles

## Development Setup

1. Install CMake and Visual Studio 2022 with C++ build tools.
2. Install Python 3.11 or newer.
3. Install `huggingface_hub`:

```powershell
python -m pip install huggingface_hub
```

4. Restore local runtime and model assets:

```powershell
.\scripts\restore-assets.ps1
```

5. Configure and build:

```powershell
cmake -S . -B build -DONNXRUNTIME_ROOT="$PWD\third_party\onnxruntime"
cmake --build build --config Release
```

## Change Guidelines

- Prefer small, focused pull requests.
- Keep the runtime self-contained.
- Do not add a Python runtime dependency to inference paths.
- Do not commit `build/`, `dist/`, generated audio files, or downloaded large ONNX models.
- Keep third-party attribution files up to date when dependency or asset handling changes.

## Before Opening A Pull Request

Please verify:

1. `cmake --build build --config Release` succeeds.
2. `.\scripts\package-release.ps1` can generate a local package after a successful build.
3. Documentation is updated if CLI flags, assets, or release steps changed.

## Pull Request Notes

Include:

- what changed
- why it changed
- how you verified it

If your change affects bundled assets, licensing, or packaging, mention that explicitly in the PR description.
