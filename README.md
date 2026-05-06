# Kokoro.C

![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Runtime](https://img.shields.io/badge/runtime-ONNX%20Runtime-green)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)
![Status](https://img.shields.io/badge/status-English--only-orange)

Kokoro.C is a standalone C++17 English text-to-speech runtime built around Kokoro ONNX models and ONNX Runtime.

It includes:

- Repository-local voice style assets
- Repository-local ONNX Runtime SDK
- A `make model` workflow for restoring large model assets locally

It does not depend on a Python bridge at runtime.

## Highlights

- Standalone C++ runtime for Kokoro ONNX inference
- Pure C++ English text front-end with a built-in phonemizer
- Optional direct phoneme input mode
- Repository-local tokenizer, voice, and runtime assets, plus local model restoration support
- Repository-friendly workflow that keeps large model binaries out of Git by default
- Basic demo and command-line example included
- Incremental `make` targets for dependency checks, build, and cleanup

## Repository Layout

- `include/kokoro_cpp.hpp`
  Public C++ API exposing `kokoro::Engine`, `SynthesisOptions`, and `AudioBuffer`
- `src/kokoro_engine.cpp`
  Runtime implementation, tokenizer loading, English text preprocessing, voice loading, ONNX inference, and WAV writing
- `examples/demo_cpp.cpp`
  Minimal English synthesis demo
- `examples/kokoro_cli.cpp`
  Command-line synthesis example
- `assets/onnx/model.onnx`
  Default Kokoro ONNX model used by the runtime, restored locally via `make model`
- `assets/tokenizer.json`
  Tokenizer vocabulary used for phoneme-to-id mapping
- `assets/voices/*.bin`
  Repository-local Kokoro voice style assets
- `lexicons/programming_terms.lexicon`
  Optional bundled pronunciation overrides for technical terms and acronyms
- `lexicons/cmudict_cache_upper.txt`
  Optional bundled CMUdict cache for broader English pronunciation coverage
- `third_party/onnxruntime/`
  Restored ONNX Runtime SDK used by the build

## Scope

- English-only
- Pure C++ runtime
- Built-in lightweight English text-to-phonemes path
- Direct phoneme input supported through `input_is_phonemes = true`
- Voice style selection from repository-local `.bin` assets

The current English front-end is practical and self-contained, but it is not intended to be token-for-token identical to `espeak-ng`, `misaki`, or the original Python stack.

## Quick Start

### 1. Verify repository-local assets

Expected paths for a ready-to-run local checkout:

- `third_party/onnxruntime/`
- `assets/tokenizer.json`
- `assets/voices/af_bella.bin`

The repository already includes multiple self-contained English voices under `assets/voices/`.
The ONNX Runtime SDK is restored locally when needed and is not required to live in Git.

### 2. Restore the model and tokenizer locally

```powershell
make model
```

This target will:

- verify `third_party/onnxruntime` exists
- download `assets/onnx/model.onnx` if missing
- download `assets/tokenizer.json` if missing
- verify the default bundled voice exists

### 3. Build everything

```powershell
make all
```

This target will:

- verify `third_party/onnxruntime` exists
- restore or verify required model assets under `assets/`
- configure and build the C++ targets

### 4. Run the basic demo

```powershell
make demo
```

This generates:

- `kokoro_cpp_demo.wav`

### 5. Run the CLI example

```powershell
make cli
```

This generates:

- `outputs/kokoro_cli.wav`

## Voices

`assets/voices/` stores Kokoro voice style binary files with the `.bin` extension.

Each `.bin` file contains the per-voice style embeddings used by the runtime to condition the Kokoro model.
At inference time, Kokoro.C selects the style row matching the current phoneme length and feeds that style vector into the ONNX model together with `input_ids` and `speed`.

Bundled voices currently include:

- `af_bella`
- `af_heart`
- `af_nicole`
- `af_sarah`
- `am_adam`
- `am_michael`
- `bf_emma`
- `bm_george`

Practical naming groups:

- `af_*` American female voices
- `am_*` American male voices
- `bf_*` British female voices
- `bm_*` British male voices

## CLI Usage

Basic usage:

```powershell
.\build\bin\Release\kokoro_cli.exe --text "Hello from Kokoro C plus plus." --output outputs\hello.wav
```

List bundled voices:

```powershell
.\build\bin\Release\kokoro_cli.exe --list-voices
```

Use a different bundled voice:

```powershell
.\build\bin\Release\kokoro_cli.exe --text "Hello from Kokoro C plus plus." --voice am_michael --output outputs\am_michael.wav
```

Pass phonemes directly:

```powershell
.\build\bin\Release\kokoro_cli.exe --text "hˌW ɑɹ ju tədˈA?" --phonemes --output outputs\phonemes.wav
```

Override the model, tokenizer, or exact voice file explicitly:

```powershell
.\build\bin\Release\kokoro_cli.exe --text "Hello from Kokoro C plus plus." --model assets\onnx\model.onnx --tokenizer assets\tokenizer.json --voice-path assets\voices\af_bella.bin --output outputs\custom.wav
```

Use a technical-term lexicon and optional CMUdict cache:

```powershell
.\build\bin\Release\kokoro_cli.exe --text "ONNX Runtime and GitHub CI help TTS projects ship faster." --g2p-lexicon lexicons\programming_terms.lexicon --cmudict lexicons\cmudict_cache_upper.txt --output outputs\tech_terms.wav
```

Use a simple happy delivery preset:

```powershell
.\build\bin\Release\kokoro_cli.exe --text "I'm so happy to see you today!" --emotion happy --emotion-strength 0.8 --output outputs\happy.wav
```

The examples above assume your current working directory is the `Kokoro.C` repository root.
If you launch the executable from a different working directory, prefer absolute paths for `--model`, `--tokenizer`, `--voice-path`, and `--output`.

## Build

```powershell
cmake -S . -B build -DONNXRUNTIME_ROOT="third_party/onnxruntime"
cmake --build build --config Release
```

The build copies the matching runtime DLLs into `build/bin/Release/` automatically:

- `onnxruntime.dll`
- `onnxruntime_providers_shared.dll`

## GitHub Release

`Kokoro.C` is ready to publish as a standalone GitHub repository.

- Source control keeps the large default model out of Git by default
- Release packaging is generated locally by script when needed
- The standalone release workflow is prepared in `.github/workflows/release.yml`
- Local release helpers are available in `scripts/restore-assets.ps1` and `scripts/package-release.ps1`

For a standalone repository, pushing a tag such as `v0.1.0` will verify that a Windows x64 release bundle can be built.
The generated `dist/` package is local build output and is not meant to be committed or uploaded automatically.
See `RELEASING.md` for the exact release process.

## API Example

```cpp
#include "kokoro_cpp.hpp"

int main() {
  kokoro::Engine engine;

  kokoro::SynthesisOptions options;
  options.voice = "af_bella";

  auto audio = engine.Synthesize("Hello from Kokoro C plus plus.", options);
  engine.SaveWav("outputs/demo.wav", audio);
}
```

Direct phoneme input example:

```cpp
#include "kokoro_cpp.hpp"

int main() {
  kokoro::Engine engine;

  kokoro::SynthesisOptions options;
  options.voice = "af_bella";
  options.input_is_phonemes = true;

  auto audio = engine.Synthesize("hˌW ɑɹ ju tədˈA?", options);
  engine.SaveWav("outputs/phonemes.wav", audio);
}
```

## Make Targets

The repository root includes a `Makefile` that wraps the most common local workflows.

```makefile
make all
make deps
make model
make build
make demo
make cli
make clean
make distclean
make purge
```

`make deps`
Checks that repository-local ONNX Runtime is present.

`make model`
Restores `model.onnx` and `tokenizer.json` locally if missing, then verifies the required runtime assets.

`make all`
Runs dependency checks, asset checks, and then builds the C++ targets.

`make demo`
Builds and runs the basic English demo.

`make cli`
Builds and runs the CLI example.

`make clean`
Removes `build/`, `outputs/`, and generated demo WAV files.

`make distclean`
Runs `clean` and removes downloaded archive files under `third_party/`.

`make purge`
Runs `distclean` and removes `assets/.cache/`.

Current C++ library target name: `kokoro_core`

## Notes

- Default sample rate is `24000`
- Default model is `assets/onnx/model.onnx`
- Default tokenizer is `assets/tokenizer.json`
- Default bundled voice is `assets/voices/af_bella.bin`
- Supported emotion presets are `neutral` and `happy`
- `emotion_strength` ranges from `0.0` to `1.0`
- Default technical lexicon is `lexicons/programming_terms.lexicon` when present
- Optional CMUdict cache is `lexicons/cmudict_cache_upper.txt` when present
- `model_q4.onnx` may still be present in `assets/onnx/`, but it is not the default runtime model

## Model And Voice Asset Licensing

This repository's C++ source code is licensed under this repository's own license.

However, Kokoro model and voice assets are third-party assets and should be treated separately from the C++ runtime code.
In practice, that means:

- `assets/onnx/model.onnx`
- `assets/tokenizer.json`
- `assets/voices/*.bin`

remain subject to their upstream licenses and attribution requirements.

For the Kokoro-82M assets currently used by this repository, the upstream model weights and the official Kokoro codebase are licensed under Apache License 2.0 (`Apache-2.0`).

For repository distribution, the recommended workflow is:

- keep C++ source and small voice assets in the repository with the required upstream attribution and license references
- restore large model binaries locally with `make model` instead of committing them to Git
- retain upstream attribution and license notes in `THIRD_PARTY.md`

Even though the upstream Kokoro assets use `Apache-2.0`, the default `model.onnx` file is still too large for normal GitHub source control storage in a standard repository workflow, so Kokoro.C restores it locally with `make model` instead of committing it to Git.

See also:

- `NOTICE`
- `THIRD_PARTY.md`

## Self-Contained Status

`Kokoro.C` is considered self-contained for local runtime use once these paths exist inside this repository tree:

- `assets/onnx/model.onnx`
- `assets/tokenizer.json`
- `assets/voices/af_bella.bin`
- `third_party/onnxruntime/`

After that, build and inference no longer need to read model or voice assets from sibling projects.
