#include "../include/kokoro_cpp.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    kokoro::Engine engine;

    kokoro::SynthesisOptions options;
    options.voice = "af_bella";
    options.speed = 1.0f;
    options.sample_rate = 24000;
    options.model_path = "assets/onnx/model.onnx";
    options.tokenizer_path = "assets/tokenizer.json";
    options.input_is_phonemes = false;

    const std::string text =
        "How are you today? I am doing reasonably well, thank you for asking.";

    kokoro::AudioBuffer audio;
    try {
        audio = engine.Synthesize(text, options);
    } catch (const std::exception& ex) {
        std::cerr << "synthesis failed: " << ex.what() << '\n';
        return 1;
    }

    std::string error;
    if (!engine.SaveWav("kokoro_cpp_demo.wav", audio, &error)) {
        std::cerr << "failed to write wav: " << error << '\n';
        return 1;
    }

    std::cout << "generated kokoro_cpp_demo.wav with " << audio.samples.size() << " samples at "
              << audio.sample_rate << " Hz\n";
    return 0;
}
