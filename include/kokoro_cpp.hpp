#ifndef KOKORO_CPP_HPP
#define KOKORO_CPP_HPP

#include <memory>
#include <string>
#include <vector>

namespace kokoro {

struct SynthesisOptions {
    std::string voice = "af_heart";
    float speed = 1.0f;
    int sample_rate = 24000;
    std::string model_path = "assets/onnx/model.onnx";
    std::string tokenizer_path = "assets/tokenizer.json";
    std::string voice_path;
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
