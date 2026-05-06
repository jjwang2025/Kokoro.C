#include "../include/kokoro_cpp.hpp"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

kokoro::EmotionPreset ParseEmotion(const std::string& value) {
    if (value == "neutral") {
        return kokoro::EmotionPreset::Neutral;
    }
    if (value == "happy") {
        return kokoro::EmotionPreset::Happy;
    }
    throw std::runtime_error("unsupported emotion preset: " + value);
}

const std::vector<std::string>& BuiltInVoices() {
    static const std::vector<std::string> voices = {
        "af_bella",
        "af_heart",
        "af_nicole",
        "af_sarah",
        "am_adam",
        "am_michael",
        "bf_emma",
        "bm_george",
    };
    return voices;
}

void PrintVoices() {
    std::cout << "Bundled voices:\n";
    for (const std::string& voice : BuiltInVoices()) {
        std::cout << "- " << voice << '\n';
    }
}

void PrintUsage() {
    std::cerr << "Usage: kokoro_cli --text \"Hello world\" [--text-file input.txt] [--output out.wav] [--voice af_bella] [--voice-path path.bin] [--model model.onnx] [--tokenizer tokenizer.json] [--cmudict path.txt] [--g2p-lexicon path.lexicon] [--speed 1.0] [--emotion neutral|happy] [--emotion-strength 0.0-1.0] [--phonemes] [--list-voices]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string text;
    std::string text_file;
    std::string output = "outputs/kokoro_cli.wav";
    std::string voice = "af_bella";
    std::string voice_path;
    std::string model_path = "assets/onnx/model.onnx";
    std::string tokenizer_path = "assets/tokenizer.json";
    std::string cmudict_path;
    std::string g2p_lexicon_path;
    float speed = 1.0f;
    float emotion_strength = 0.5f;
    kokoro::EmotionPreset emotion = kokoro::EmotionPreset::Neutral;
    bool input_is_phonemes = false;
    bool list_voices = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--text" && i + 1 < argc) {
            text = argv[++i];
        } else if (arg == "--text-file" && i + 1 < argc) {
            text_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output = argv[++i];
        } else if (arg == "--voice" && i + 1 < argc) {
            voice = argv[++i];
        } else if (arg == "--voice-path" && i + 1 < argc) {
            voice_path = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--tokenizer" && i + 1 < argc) {
            tokenizer_path = argv[++i];
        } else if (arg == "--cmudict" && i + 1 < argc) {
            cmudict_path = argv[++i];
        } else if (arg == "--g2p-lexicon" && i + 1 < argc) {
            g2p_lexicon_path = argv[++i];
        } else if (arg == "--speed" && i + 1 < argc) {
            speed = std::stof(argv[++i]);
        } else if (arg == "--emotion" && i + 1 < argc) {
            emotion = ParseEmotion(argv[++i]);
        } else if (arg == "--emotion-strength" && i + 1 < argc) {
            emotion_strength = std::stof(argv[++i]);
        } else if (arg == "--phonemes") {
            input_is_phonemes = true;
        } else if (arg == "--list-voices") {
            list_voices = true;
        } else {
            PrintUsage();
            return 1;
        }
    }

    if (list_voices) {
        PrintVoices();
        return 0;
    }

    if (!text_file.empty()) {
        std::ifstream input(text_file, std::ios::binary);
        if (!input) {
            std::cerr << "failed to read text file: " << text_file << '\n';
            return 1;
        }
        text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    if (text.empty()) {
        PrintUsage();
        return 1;
    }

    kokoro::SynthesisOptions options;
    options.voice = voice;
    options.voice_path = voice_path;
    options.speed = speed;
    options.emotion = emotion;
    options.emotion_strength = emotion_strength;
    options.sample_rate = 24000;
    options.model_path = model_path;
    options.tokenizer_path = tokenizer_path;
    options.cmudict_path = cmudict_path;
    options.g2p_lexicon_path = g2p_lexicon_path;
    options.input_is_phonemes = input_is_phonemes;

    try {
        kokoro::Engine engine;
        const kokoro::AudioBuffer audio = engine.Synthesize(text, options);

        std::string error;
        if (!engine.SaveWav(output, audio, &error)) {
            std::cerr << "failed to write wav: " << error << '\n';
            return 1;
        }

        std::cout << "generated " << output << " with " << audio.samples.size() << " samples at "
                  << audio.sample_rate << " Hz\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "synthesis failed: " << ex.what() << '\n';
        return 1;
    }
}
