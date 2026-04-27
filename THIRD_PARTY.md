# THIRD_PARTY

This repository relies on a small set of third-party runtime components and model assets.

## Runtime Dependency

### ONNX Runtime

Kokoro.C links against a locally restored ONNX Runtime SDK under:

```text
third_party/onnxruntime/
```

The build currently expects the extracted package layout that contains:

- `build/native/include/onnxruntime_cxx_api.h`
- `runtimes/win-x64/native/onnxruntime.lib`
- `runtimes/win-x64/native/onnxruntime.dll`
- `runtimes/win-x64/native/onnxruntime_providers_shared.dll`

The CMake build copies the required runtime DLLs into `build/bin/Release/` automatically.
The extracted SDK is reproducible from `scripts/restore-assets.ps1` and does not need to be committed to Git.

## Model Assets

### Kokoro ONNX Model

The default acoustic runtime model is stored at:

```text
assets/onnx/model.onnx
```

Current restoration source used by `make model`:

- Hugging Face: `onnx-community/Kokoro-82M-v1.0-ONNX`

License status used by this repository:

- Upstream Kokoro-82M model weights: Apache License 2.0 (`Apache-2.0`)

### Tokenizer Vocabulary

The tokenizer vocabulary used for phoneme-to-id mapping is stored at:

```text
assets/tokenizer.json
```

Current restoration source used by `make model`:

- Hugging Face: `onnx-community/Kokoro-82M-v1.0-ONNX`

License status used by this repository:

- Distributed together with the Kokoro-82M ONNX asset set under Apache License 2.0 (`Apache-2.0`)

### Voice Style Assets

Repository-local Kokoro voice style assets are stored under:

```text
assets/voices/*.bin
```

These `.bin` files contain the voice-specific style embeddings used to condition the Kokoro ONNX model during inference.

The currently bundled voice files were copied from the upstream Kokoro voice asset set and should be treated as third-party assets.

License status used by this repository:

- Upstream Kokoro-82M voice assets: Apache License 2.0 (`Apache-2.0`)

### Optional Pronunciation Lexicons

`Kokoro.C` can also consult local pronunciation lexicons such as:

```text
lexicons/programming_terms.lexicon
lexicons/cmudict_cache_upper.txt
```

These files are optional text-processing assets used to improve pronunciation stability for technical terms, acronyms, and named entities.

## Redistribution And License Notes

This repository's source code license does not automatically replace the upstream license terms of model or voice assets.

For practical repository maintenance:

- large model binaries such as `assets/onnx/model.onnx` should be restored locally rather than committed to Git
- voice assets should only be redistributed if their upstream license terms allow it
- upstream attribution, license text, and any notice requirements should be preserved when redistributing those assets

At the time of writing, Kokoro.C uses upstream Kokoro model and voice assets obtained from public upstream distribution sources and treats them as Apache License 2.0 (`Apache-2.0`) assets.

Even with a permissive upstream license, the repository still avoids committing `assets/onnx/model.onnx` directly because the file is larger than GitHub's standard single-file source control limit.

## Notes

- Runtime inference does not depend on a Python bridge
- Repository-local assets are sufficient for build and inference once present
- See `README.md` for expected repository layout and local workflow
