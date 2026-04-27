BUILD_DIR := build
THIRD_PARTY_DIR := third_party
ASSETS_DIR := assets
OUTPUT_DIR := outputs
ONNXRUNTIME_PACKAGE := onnxruntime
ONNXRUNTIME_ROOT ?= $(CURDIR)/$(THIRD_PARTY_DIR)/$(ONNXRUNTIME_PACKAGE)
ONNXRUNTIME_ZIP := $(THIRD_PARTY_DIR)/onnxruntime.zip
ONNXRUNTIME_NUPKG := $(THIRD_PARTY_DIR)/onnxruntime.nupkg
ONNXRUNTIME_URL := https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.23.1/microsoft.ml.onnxruntime.1.23.1.nupkg
KOKORO_REPO := onnx-community/Kokoro-82M-v1.0-ONNX
MODEL_FILE := $(ASSETS_DIR)/onnx/model.onnx
TOKENIZER_FILE := $(ASSETS_DIR)/tokenizer.json
CONFIGURE_STAMP := $(BUILD_DIR)/CMakeCache.txt
DEMO_EXE := $(BUILD_DIR)/bin/Release/kokoro_cpp_demo.exe
CLI_EXE := $(BUILD_DIR)/bin/Release/kokoro_cli.exe

.PHONY: all deps model ensure-runtime ensure-assets ensure-model ensure-tokenizer configure build demo cli clean distclean purge

all: deps model build

deps: ensure-runtime

model: deps ensure-model ensure-tokenizer ensure-assets

ensure-model:
	powershell -NoProfile -Command "if (!(Test-Path '$(ASSETS_DIR)/onnx')) { New-Item -ItemType Directory -Path '$(ASSETS_DIR)/onnx' | Out-Null }"
	powershell -NoProfile -Command "if (!(Test-Path '$(MODEL_FILE)')) { python -c \"from huggingface_hub import hf_hub_download; print(hf_hub_download('$(KOKORO_REPO)','onnx/model.onnx', local_dir=r'$(CURDIR)/$(ASSETS_DIR)'))\" }"

ensure-tokenizer:
	powershell -NoProfile -Command "if (!(Test-Path '$(TOKENIZER_FILE)')) { python -c \"from huggingface_hub import hf_hub_download; print(hf_hub_download('$(KOKORO_REPO)','tokenizer.json', local_dir=r'$(CURDIR)/$(ASSETS_DIR)'))\" }"

ensure-runtime:
	powershell -NoProfile -Command "if (!(Test-Path '$(THIRD_PARTY_DIR)')) { New-Item -ItemType Directory -Path '$(THIRD_PARTY_DIR)' | Out-Null }"
	powershell -NoProfile -Command "if (!(Test-Path '$(ONNXRUNTIME_ROOT)')) { if (!(Test-Path '$(ONNXRUNTIME_NUPKG)')) { Invoke-WebRequest -Uri '$(ONNXRUNTIME_URL)' -OutFile '$(ONNXRUNTIME_NUPKG)' }; Copy-Item '$(ONNXRUNTIME_NUPKG)' '$(ONNXRUNTIME_ZIP)' -Force; Expand-Archive -Path '$(ONNXRUNTIME_ZIP)' -DestinationPath '$(ONNXRUNTIME_ROOT)' -Force }"
	powershell -NoProfile -Command "if (!(Test-Path '$(ONNXRUNTIME_ROOT)/build/native/include/onnxruntime_cxx_api.h')) { throw 'Missing ONNX Runtime headers under $(ONNXRUNTIME_ROOT)' }"
	powershell -NoProfile -Command "if (!(Test-Path '$(ONNXRUNTIME_ROOT)/runtimes/win-x64/native/onnxruntime.lib')) { throw 'Missing ONNX Runtime import library under $(ONNXRUNTIME_ROOT)' }"

ensure-assets:
	powershell -NoProfile -Command "if (!(Test-Path '$(MODEL_FILE)')) { throw 'Missing $(MODEL_FILE)' }"
	powershell -NoProfile -Command "if (!(Test-Path '$(TOKENIZER_FILE)')) { throw 'Missing $(TOKENIZER_FILE)' }"
	powershell -NoProfile -Command "if (!(Test-Path '$(ASSETS_DIR)/voices/af_bella.bin')) { throw 'Missing default voice $(ASSETS_DIR)/voices/af_bella.bin' }"

build:

configure: $(CONFIGURE_STAMP)

$(CONFIGURE_STAMP): CMakeLists.txt Makefile
	powershell -NoProfile -Command "if (!(Test-Path '$(BUILD_DIR)')) { New-Item -ItemType Directory -Path '$(BUILD_DIR)' | Out-Null }"
	cmake -S . -B $(BUILD_DIR) -DONNXRUNTIME_ROOT="$(ONNXRUNTIME_ROOT)"

$(DEMO_EXE): $(CONFIGURE_STAMP) include/kokoro_cpp.hpp src/kokoro_engine.cpp examples/demo_cpp.cpp
	cmake --build $(BUILD_DIR) --config Release --target kokoro_cpp_demo

$(CLI_EXE): $(CONFIGURE_STAMP) include/kokoro_cpp.hpp src/kokoro_engine.cpp examples/kokoro_cli.cpp
	cmake --build $(BUILD_DIR) --config Release --target kokoro_cli

build: deps $(DEMO_EXE) $(CLI_EXE)

demo: model $(DEMO_EXE)
	powershell -NoProfile -Command "& '$(CURDIR)\$(BUILD_DIR)\bin\Release\kokoro_cpp_demo.exe'"

cli: model $(CLI_EXE)
	powershell -NoProfile -Command "if (!(Test-Path '$(OUTPUT_DIR)')) { New-Item -ItemType Directory -Path '$(OUTPUT_DIR)' | Out-Null }; & '$(CURDIR)\$(BUILD_DIR)\bin\Release\kokoro_cli.exe' --text 'Hello from Kokoro C plus plus.' --output '$(OUTPUT_DIR)/kokoro_cli.wav'"

clean:
	powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item '$(BUILD_DIR)' -Recurse -Force }"
	powershell -NoProfile -Command "if (Test-Path '$(OUTPUT_DIR)') { Remove-Item '$(OUTPUT_DIR)' -Recurse -Force }"
	powershell -NoProfile -Command "if (Test-Path 'kokoro_cpp_demo.wav') { Remove-Item 'kokoro_cpp_demo.wav' -Force }"

distclean: clean
	powershell -NoProfile -Command "if (Test-Path '$(ONNXRUNTIME_ZIP)') { Remove-Item '$(ONNXRUNTIME_ZIP)' -Force }"
	powershell -NoProfile -Command "if (Test-Path '$(ONNXRUNTIME_NUPKG)') { Remove-Item '$(ONNXRUNTIME_NUPKG)' -Force }"

purge: distclean
	powershell -NoProfile -Command "if (Test-Path '$(ASSETS_DIR)/.cache') { Remove-Item '$(ASSETS_DIR)/.cache' -Recurse -Force }"
