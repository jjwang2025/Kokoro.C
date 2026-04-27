# Standalone Repo Checklist

This checklist is for publishing `Kokoro.C` as its own GitHub repository.

## Copy As Repository Root

Use `Kokoro.C/` itself as the new repository root.

Keep these paths:

- `.github/`
- `.gitignore`
- `assets/`
- `examples/`
- `include/`
- `scripts/`
- `src/`
- `CHANGELOG.md`
- `CMakeLists.txt`
- `CONTRIBUTING.md`
- `LICENSE`
- `Makefile`
- `NOTICE`
- `README.md`
- `RELEASING.md`
- `SECURITY.md`
- `STANDALONE_REPO_CHECKLIST.md`
- `THIRD_PARTY.md`

## Keep Or Exclude

Recommended to keep in Git:

- `assets/voices/*.bin`

Why:

- the full bundled voice set is only about 4 MB
- the runtime expects local voice files by default
- keeping voices in Git makes the repository usable immediately after model restore

Recommended to exclude from Git:

- `third_party/onnxruntime/`

Why:

- the extracted SDK is about 213 MB
- it is reproducible from `scripts/restore-assets.ps1`
- committing it makes the repository much heavier without adding source-level value
- the build already supports `ONNXRUNTIME_ROOT`, so the SDK can be restored locally when needed

## Do Not Carry Over

Do not copy workspace-level or local generated content into the new repository:

- the parent repository `.git/`
- sibling projects outside `Kokoro.C/`
- `build*/`
- `dist/`
- `outputs/`
- `kokoro_cpp_demo.wav`
- `assets/.cache/`
- `assets/onnx/model.onnx`
- `assets/onnx/model_q4.onnx`
- `assets/onnx/model_uint8.onnx`
- `third_party/onnxruntime/`
- `third_party/onnxruntime.zip`
- `third_party/onnxruntime.nupkg`

## Recommended First Commit Scope

The first standalone repository commit should contain:

- source code
- headers
- examples
- scripts
- docs
- GitHub templates and workflow
- bundled voice assets

It should not contain local build output, downloaded ONNX model binaries, or the extracted ONNX Runtime SDK.

## New Repository Steps

1. Create a new empty GitHub repository for `Kokoro.C`.
2. Copy the contents of `Kokoro.C/` into a clean folder.
3. Initialize Git in that folder.
4. Verify `.gitignore` is present before staging files.
5. Review staged files and confirm no build output or large local model files are included.
6. Confirm `third_party/onnxruntime/` is not staged.
7. Create the initial commit.
8. Push to the new remote repository.

## Example Local Commands

```powershell
mkdir Kokoro.C.repo
Copy-Item -Recurse -Force .\Kokoro.C\* .\Kokoro.C.repo\
cd .\Kokoro.C.repo
git init
git status --short
git add .
git status --short
git commit -m "Initial standalone Kokoro.C repository"
git branch -M main
git remote add origin <your-repo-url>
git push -u origin main
```

## Post-Migration Checks

After pushing the standalone repository, verify:

1. `README.md` renders correctly on GitHub.
2. `.github/ISSUE_TEMPLATE/` and PR template are active.
3. `.github/workflows/release.yml` appears in the Actions tab.
4. `python -m pip install huggingface_hub` works in your environment.
5. `.\scripts\restore-assets.ps1` succeeds.
6. `third_party/onnxruntime/` is restored locally by the script and remains untracked.
7. `cmake -S . -B build -DONNXRUNTIME_ROOT="$PWD\third_party\onnxruntime"` succeeds.
8. `cmake --build build --config Release` succeeds.
9. `.\scripts\package-release.ps1 -Version v0.1.0` generates a local package under `dist/`.

## Release Tag Check

When the standalone repository is ready:

1. create a tag such as `v0.1.0`
2. push the tag
3. confirm the GitHub Action completes successfully

The workflow is currently intended to verify that packaging works. It does not upload the local package automatically.
