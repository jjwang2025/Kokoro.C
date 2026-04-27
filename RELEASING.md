# Releasing

This project is ready to be published as a standalone GitHub repository.

## Release Shape

The default Windows release bundle contains:

- `kokoro_cli.exe`
- `kokoro_cpp_demo.exe`
- `onnxruntime.dll`
- `onnxruntime_providers_shared.dll`
- `assets/onnx/model.onnx`
- `assets/tokenizer.json`
- `assets/voices/*.bin`
- `README.md`
- `LICENSE`
- `NOTICE`
- `THIRD_PARTY.md`
- `CHANGELOG.md`

## Standalone Repository Setup

If `Kokoro.C/` is still stored inside a larger workspace, publish it with `Kokoro.C` as the repository root.

Files already prepared for a standalone repository:

- `.github/workflows/release.yml`
- `scripts/restore-assets.ps1`
- `scripts/package-release.ps1`
- `LICENSE`
- `CHANGELOG.md`
- `STANDALONE_REPO_CHECKLIST.md`

## Tag Release

1. Push the project as its own repository with `Kokoro.C` as the root directory.
2. Create and push a version tag such as `v0.1.0`.
3. GitHub Actions verifies that the Windows release bundle can be generated successfully.

## Manual Local Package

```powershell
python -m pip install huggingface_hub
.\scripts\restore-assets.ps1
cmake -S . -B build -DONNXRUNTIME_ROOT="$PWD\third_party\onnxruntime"
cmake --build build --config Release
.\scripts\package-release.ps1 -Version v0.1.0
```

## Notes

- `assets/onnx/model.onnx` stays out of Git by default.
- The default model is about 310 MB, which is too large for normal source control but acceptable for GitHub Releases.
- `dist/` is local build output from `scripts/package-release.ps1` and should not be committed.
- If you keep this project inside a monorepo, the workflow file must be moved to the repository root before GitHub Actions can run it.
- See `STANDALONE_REPO_CHECKLIST.md` for the exact split-out steps.
