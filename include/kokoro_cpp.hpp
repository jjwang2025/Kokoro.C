#ifndef KOKORO_CPP_HPP
#define KOKORO_CPP_HPP

#include <memory>
#include <string>
#include <vector>

namespace kokoro {

enum class EmotionPreset {
    Neutral,
    Happy,
};

struct SynthesisOptions {
    std::string voice = "af_heart";
    float speed = 1.0f;
    float emotion_strength = 0.5f;
    float breath_strength = 0.35f;
    int sample_rate = 24000;
    std::string model_path = "assets/onnx/model.onnx";
    std::string tokenizer_path = "assets/tokenizer.json";
    std::string cmudict_path;
    std::string g2p_lexicon_path;
    std::string voice_path;
    EmotionPreset emotion = EmotionPreset::Neutral;
    bool enable_breaths = false;
    bool input_is_phonemes = false;
};

struct AudioBuffer {
    std::vector<float> samples;
    int sample_rate = 24000;
};

class Engine {
public:
    Engine();
    ~Engine();

    Engine(Engine&& other) noexcept;
    Engine& operator=(Engine&& other) noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    AudioBuffer Synthesize(const std::string& text, const SynthesisOptions& options = {});
    bool SaveWav(const std::string& path, const AudioBuffer& audio, std::string* error = nullptr) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kokoro

#endif
