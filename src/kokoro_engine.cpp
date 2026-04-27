#include "../include/kokoro_cpp.hpp"
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kokoro {
namespace {

constexpr int kSampleRate = 24000;
constexpr std::size_t kStyleDim = 256;

const std::unordered_map<std::string, std::string>& EnglishLexicon() {
    static const std::unordered_map<std::string, std::string> lexicon = {
        {"a", "ɐ"},
        {"am", "ɐm"},
        {"and", "ænd"},
        {"are", "ɑɹ"},
        {"ask", "ˈæsk"},
        {"asking", "ˈæskɪŋ"},
        {"bella", "bˈɛlə"},
        {"build", "bˈɪld"},
        {"c", "sˈiː"},
        {"demo", "dˈɛmoʊ"},
        {"doing", "dˈuɪŋ"},
        {"english", "ˈɪŋɡlɪʃ"},
        {"example", "ɛɡzˈæmpəl"},
        {"for", "fɔɹ"},
        {"hello", "həlˈoʊ"},
        {"how", "hˌW"},
        {"i", "ˌI"},
        {"is", "ˈɪz"},
        {"kokoro", "kˈoʊkəɹoʊ"},
        {"local", "lˈoʊkəl"},
        {"on", "ˈɑːn"},
        {"reasonably", "ɹˈizənəbli"},
        {"runs", "ɹˈʌnz"},
        {"simple", "sˈɪmpəl"},
        {"test", "tˈɛst"},
        {"text", "tˈɛkst"},
        {"thank", "θˈæŋk"},
        {"this", "ðˈɪs"},
        {"today", "tədˈA"},
        {"well", "wˈɛl"},
        {"with", "wˈɪð"},
        {"world", "wˈɜːld"},
        {"you", "ju"},
    };
    return lexicon;
}

bool FileExists(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return stream.good();
}

std::string ReadFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to open file: " + path);
    }

    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string Trim(const std::string& text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(start, end - start);
}

std::string ToLowerAscii(const std::string& text) {
    std::string lowered = text;
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

std::string NormalizeEnglishText(const std::string& text) {
    std::string normalized = text;
    normalized = std::regex_replace(normalized, std::regex("[‘’]"), "'");
    normalized = std::regex_replace(normalized, std::regex("[“”]"), "\"");
    normalized = std::regex_replace(normalized, std::regex("\\b[Dd][Rr]\\.(?= [A-Z])"), "Doctor");
    normalized = std::regex_replace(normalized, std::regex("\\b(?:Mr\\.|MR\\.(?= [A-Z]))"), "Mister");
    normalized = std::regex_replace(normalized, std::regex("\\b(?:Ms\\.|MS\\.(?= [A-Z]))"), "Miss");
    normalized = std::regex_replace(normalized, std::regex("\\b(?:Mrs\\.|MRS\\.(?= [A-Z]))"), "Mrs");
    normalized = std::regex_replace(normalized, std::regex("\\betc\\.(?! [A-Z])", std::regex::icase), "etc");
    normalized = std::regex_replace(normalized, std::regex("[-/]+"), " ");
    normalized = std::regex_replace(normalized, std::regex("[^ -~]"), " ");
    normalized = std::regex_replace(normalized, std::regex("[\\t\\r\\n]+"), " ");
    normalized = std::regex_replace(normalized, std::regex(" +"), " ");
    return Trim(normalized);
}

bool IsPunctuationToken(const std::string& token) {
    return token == "." || token == "," || token == "!" || token == "?" || token == ":" || token == ";";
}

std::string MapPunctuationPhone(const std::string& token) {
    if (token == ":" || token == ";") {
        return ",";
    }
    return token;
}

bool IsWordChar(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '\'';
}

std::vector<std::string> SplitWordsKeepingPunctuation(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;

    auto flush = [&parts, &current]() {
        if (!current.empty()) {
            parts.push_back(current);
            current.clear();
        }
    };

    for (char ch : text) {
        if (IsWordChar(ch)) {
            current.push_back(ch);
            continue;
        }

        flush();
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            parts.push_back(" ");
        } else {
            parts.emplace_back(1, ch);
        }
    }
    flush();
    return parts;
}

std::string CollapseSpacesAroundPunctuation(const std::string& phonemes) {
    std::string output = std::regex_replace(phonemes, std::regex(" +([;:,.!?])"), "$1");
    output = std::regex_replace(output, std::regex("([;:,.!?])(?![ ]|$)"), "$1 ");
    output = std::regex_replace(output, std::regex(" +"), " ");
    return Trim(output);
}

std::string FallbackWordToPhonemes(const std::string& word) {
    std::string lower = ToLowerAscii(word);

    if (lower == "c++") {
        return "sˈiː plˈʌs plˈʌs";
    }
    if (lower == "mr") {
        return "mˈɪstɚ";
    }
    if (lower == "ms") {
        return "mˈɪs";
    }
    if (lower == "mrs") {
        return "mˈɪsɪz";
    }
    if (lower == "dr") {
        return "dˈɑːktɚ";
    }

    static const std::unordered_map<char, std::string> letter_map = {
        {'a', "æ"}, {'b', "b"}, {'c', "k"}, {'d', "d"}, {'e', "ɛ"}, {'f', "f"}, {'g', "ɡ"},
        {'h', "h"}, {'i', "ɪ"}, {'j', "dʒ"}, {'k', "k"}, {'l', "l"}, {'m', "m"}, {'n', "n"},
        {'o', "oʊ"}, {'p', "p"}, {'q', "k"}, {'r', "ɹ"}, {'s', "s"}, {'t', "t"}, {'u', "u"},
        {'v', "v"}, {'w', "w"}, {'x', "ks"}, {'y', "j"}, {'z', "z"},
    };

    std::string phonemes;
    bool first = true;
    for (char ch : lower) {
        if (ch == '\'') {
            continue;
        }
        const auto it = letter_map.find(ch);
        if (it == letter_map.end()) {
            continue;
        }
        if (!first) {
            phonemes.push_back(' ');
        }
        phonemes += it->second;
        first = false;
    }
    return phonemes.empty() ? "tˈɛkst" : phonemes;
}

std::string PhonemizeEnglishText(const std::string& text) {
    const std::string normalized = NormalizeEnglishText(text);
    const auto parts = SplitWordsKeepingPunctuation(normalized);
    const auto& lexicon = EnglishLexicon();

    std::string result;
    for (const std::string& part : parts) {
        if (part.empty()) {
            continue;
        }

        if (part == " ") {
            if (!result.empty() && result.back() != ' ') {
                result.push_back(' ');
            }
            continue;
        }

        if (IsPunctuationToken(part)) {
            result += MapPunctuationPhone(part);
            continue;
        }

        const std::string lower = ToLowerAscii(part);
        const auto it = lexicon.find(lower);
        if (it != lexicon.end()) {
            result += it->second;
        } else {
            result += FallbackWordToPhonemes(part);
        }
        result.push_back(' ');
    }

    result = std::regex_replace(result, std::regex("k ə k ˈo ʊ k ə ɹ o ʊ"), "kˈoʊkəɹoʊ");
    result = CollapseSpacesAroundPunctuation(result);
    result = std::regex_replace(result, std::regex("r"), "ɹ");
    return result;
}

float ClampSpeed(float speed) {
    if (speed < 0.5f) {
        return 0.5f;
    }
    if (speed > 2.0f) {
        return 2.0f;
    }
    return speed;
}

float NormalizeAmplitude(float value) {
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

std::wstring ToWide(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

std::string ResolveVoiceFile(const SynthesisOptions& options) {
    if (!options.voice_path.empty()) {
        return options.voice_path;
    }
    if (options.voice.find('/') != std::string::npos || options.voice.find('\\') != std::string::npos) {
        return options.voice;
    }

    const std::string local_voice = "assets/voices/" + options.voice + ".bin";
    if (FileExists(local_voice)) {
        return local_voice;
    }

    return "../kokoro.js/voices/" + options.voice + ".bin";
}

std::string DecodeJsonString(const std::string& escaped) {
    std::string output;
    output.reserve(escaped.size());

    for (std::size_t i = 0; i < escaped.size(); ++i) {
        const char ch = escaped[i];
        if (ch != '\\') {
            output.push_back(ch);
            continue;
        }

        if (i + 1 >= escaped.size()) {
            throw std::runtime_error("invalid JSON escape sequence");
        }

        const char next = escaped[++i];
        switch (next) {
        case '\\': output.push_back('\\'); break;
        case '"': output.push_back('"'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u': {
            if (i + 4 >= escaped.size()) {
                throw std::runtime_error("invalid unicode escape sequence");
            }

            const std::string hex = escaped.substr(i + 1, 4);
            const unsigned int codepoint = static_cast<unsigned int>(std::stoul(hex, nullptr, 16));
            i += 4;

            if (codepoint <= 0x7f) {
                output.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7ff) {
                output.push_back(static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
            } else {
                output.push_back(static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
            }
            break;
        }
        default:
            throw std::runtime_error("unsupported JSON escape sequence");
        }
    }

    return output;
}

std::size_t FindMatchingBrace(const std::string& text, std::size_t open_brace) {
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (std::size_t i = open_brace; i < text.size(); ++i) {
        const char ch = text[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    throw std::runtime_error("failed to find matching brace in tokenizer.json");
}

std::vector<std::pair<std::string, std::int64_t>> ParseVocab(const std::string& tokenizer_json) {
    const std::string marker = "\"vocab\"";
    const std::size_t vocab_pos = tokenizer_json.find(marker);
    if (vocab_pos == std::string::npos) {
        throw std::runtime_error("tokenizer.json does not contain model.vocab");
    }

    const std::size_t open_brace = tokenizer_json.find('{', vocab_pos);
    if (open_brace == std::string::npos) {
        throw std::runtime_error("invalid tokenizer.json vocab object");
    }

    const std::size_t close_brace = FindMatchingBrace(tokenizer_json, open_brace);
    const std::string vocab_body = tokenizer_json.substr(open_brace + 1, close_brace - open_brace - 1);

    std::vector<std::pair<std::string, std::int64_t>> vocab;
    std::size_t i = 0;
    while (i < vocab_body.size()) {
        while (i < vocab_body.size() && (vocab_body[i] == ' ' || vocab_body[i] == '\n' || vocab_body[i] == '\r' || vocab_body[i] == '\t' || vocab_body[i] == ',')) {
            ++i;
        }
        if (i >= vocab_body.size()) {
            break;
        }
        if (vocab_body[i] != '"') {
            throw std::runtime_error("invalid tokenizer vocab entry");
        }

        ++i;
        std::string key_escaped;
        bool escaped = false;
        for (; i < vocab_body.size(); ++i) {
            const char ch = vocab_body[i];
            if (escaped) {
                key_escaped.push_back('\\');
                key_escaped.push_back(ch);
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                ++i;
                break;
            }
            key_escaped.push_back(ch);
        }

        while (i < vocab_body.size() && (vocab_body[i] == ' ' || vocab_body[i] == ':')) {
            ++i;
        }

        const std::size_t value_start = i;
        while (i < vocab_body.size() && vocab_body[i] >= '0' && vocab_body[i] <= '9') {
            ++i;
        }
        if (value_start == i) {
            throw std::runtime_error("invalid tokenizer vocab value");
        }

        vocab.emplace_back(DecodeJsonString(key_escaped), std::stoll(vocab_body.substr(value_start, i - value_start)));
    }

    if (vocab.empty()) {
        throw std::runtime_error("tokenizer vocab is empty");
    }

    return vocab;
}

std::vector<std::string> SplitUtf8Codepoints(const std::string& text) {
    std::vector<std::string> codepoints;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        if ((lead & 0x80u) == 0x00u) {
            length = 1;
        } else if ((lead & 0xe0u) == 0xc0u) {
            length = 2;
        } else if ((lead & 0xf0u) == 0xe0u) {
            length = 3;
        } else if ((lead & 0xf8u) == 0xf0u) {
            length = 4;
        } else {
            throw std::runtime_error("invalid UTF-8 sequence in phoneme input");
        }

        if (i + length > text.size()) {
            throw std::runtime_error("truncated UTF-8 sequence in phoneme input");
        }

        codepoints.emplace_back(text.substr(i, length));
        i += length;
    }
    return codepoints;
}

std::vector<float> ReadVoiceFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to open voice file: " + path);
    }

    stream.seekg(0, std::ios::end);
    const std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    if (size <= 0 || (size % static_cast<std::streamsize>(sizeof(float))) != 0) {
        throw std::runtime_error("invalid voice file size: " + path);
    }

    std::vector<float> data(static_cast<std::size_t>(size) / sizeof(float));
    stream.read(reinterpret_cast<char*>(data.data()), size);
    if (!stream) {
        throw std::runtime_error("failed to read voice file: " + path);
    }
    return data;
}

bool WriteLittleEndian(std::ofstream& stream, std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        stream.put(static_cast<char>((value >> (8 * i)) & 0xff));
    }
    return stream.good();
}

}  // namespace

class Engine::Impl {
public:
    Impl()
        : env(ORT_LOGGING_LEVEL_WARNING, "kokoro") {
    }

    void EnsureLoaded(const SynthesisOptions& options) {
        if (options.sample_rate != 24000) {
            throw std::runtime_error("Kokoro ONNX outputs fixed 24000 Hz audio");
        }

        const std::string resolved_voice_path = ResolveVoiceFile(options);
        if (loaded_model_path == options.model_path &&
            loaded_tokenizer_path == options.tokenizer_path &&
            loaded_voice_path == resolved_voice_path &&
            session != nullptr) {
            return;
        }

        if (!FileExists(options.model_path)) {
            throw std::runtime_error("model file not found: " + options.model_path);
        }
        if (!FileExists(options.tokenizer_path)) {
            throw std::runtime_error("tokenizer file not found: " + options.tokenizer_path);
        }
        if (!FileExists(resolved_voice_path)) {
            throw std::runtime_error("voice file not found: " + resolved_voice_path);
        }

        tokenizer_vocab = ParseVocab(ReadFile(options.tokenizer_path));
        voice_data = ReadVoiceFile(resolved_voice_path);

        const std::size_t required_style_frames = 510 * kStyleDim;
        if (voice_data.size() < required_style_frames) {
            throw std::runtime_error("voice file does not contain enough style frames");
        }

        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        session = std::make_unique<Ort::Session>(env, ToWide(options.model_path).c_str(), session_options);

        loaded_model_path = options.model_path;
        loaded_tokenizer_path = options.tokenizer_path;
        loaded_voice_path = resolved_voice_path;
    }

    std::vector<std::int64_t> TokenizePhonemes(const std::string& phonemes) const {
        std::vector<std::int64_t> input_ids;
        input_ids.reserve(phonemes.size() + 2);
        input_ids.push_back(0);

        const std::vector<std::string> codepoints = SplitUtf8Codepoints(phonemes);
        for (const std::string& codepoint : codepoints) {
            bool matched = false;
            for (const auto& entry : tokenizer_vocab) {
                if (entry.first == codepoint) {
                    input_ids.push_back(entry.second);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                std::ostringstream stream;
                stream << "tokenizer vocab does not contain codepoint in phoneme input: " << codepoint;
                throw std::runtime_error(stream.str());
            }
        }

        input_ids.push_back(0);
        if (input_ids.size() > 512) {
            throw std::runtime_error("phoneme input exceeds Kokoro context length");
        }
        return input_ids;
    }

    std::vector<float> SelectStyle(const std::vector<std::int64_t>& input_ids) const {
        const std::size_t phoneme_count = input_ids.size() >= 2 ? (input_ids.size() - 2) : 0;
        const std::size_t style_index = std::min<std::size_t>(phoneme_count, 509);
        const std::size_t offset = style_index * kStyleDim;

        if (offset + kStyleDim > voice_data.size()) {
            throw std::runtime_error("voice style index out of range");
        }

        return std::vector<float>(voice_data.begin() + static_cast<std::ptrdiff_t>(offset), voice_data.begin() + static_cast<std::ptrdiff_t>(offset + kStyleDim));
    }

    AudioBuffer Run(const std::string& text, const SynthesisOptions& options) {
        EnsureLoaded(options);

        const std::string phonemes = options.input_is_phonemes ? text : PhonemizeEnglishText(text);
        std::vector<std::int64_t> input_ids = TokenizePhonemes(phonemes);
        std::vector<float> style = SelectStyle(input_ids);
        float speed = ClampSpeed(options.speed);

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        const std::array<std::int64_t, 2> input_shape = {1, static_cast<std::int64_t>(input_ids.size())};
        const std::array<std::int64_t, 2> style_shape = {1, static_cast<std::int64_t>(kStyleDim)};
        const std::array<std::int64_t, 1> speed_shape = {1};

        Ort::Value input_ids_tensor = Ort::Value::CreateTensor<std::int64_t>(memory_info, input_ids.data(), input_ids.size(), input_shape.data(), input_shape.size());
        Ort::Value style_tensor = Ort::Value::CreateTensor<float>(memory_info, style.data(), style.size(), style_shape.data(), style_shape.size());
        Ort::Value speed_tensor = Ort::Value::CreateTensor<float>(memory_info, &speed, 1, speed_shape.data(), speed_shape.size());

        std::array<Ort::Value, 3> inputs = {std::move(input_ids_tensor), std::move(style_tensor), std::move(speed_tensor)};

        static constexpr const char* kInputNames[] = {"input_ids", "style", "speed"};
        static constexpr const char* kOutputNames[] = {"waveform"};

        auto outputs = session->Run(Ort::RunOptions{nullptr}, kInputNames, inputs.data(), inputs.size(), kOutputNames, 1);
        if (outputs.empty()) {
            throw std::runtime_error("ONNX Runtime returned no outputs");
        }

        Ort::Value& waveform = outputs.front();
        const auto tensor_info = waveform.GetTensorTypeAndShapeInfo();
        const std::size_t sample_count = tensor_info.GetElementCount();
        const float* raw_samples = waveform.GetTensorData<float>();

        AudioBuffer audio;
        audio.sample_rate = kSampleRate;
        audio.samples.assign(raw_samples, raw_samples + sample_count);
        return audio;
    }

private:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    std::vector<std::pair<std::string, std::int64_t>> tokenizer_vocab;
    std::vector<float> voice_data;
    std::string loaded_model_path;
    std::string loaded_tokenizer_path;
    std::string loaded_voice_path;
};

Engine::Engine()
    : impl_(std::make_unique<Impl>()) {
}

Engine::~Engine() = default;

Engine::Engine(Engine&& other) noexcept = default;

Engine& Engine::operator=(Engine&& other) noexcept = default;

AudioBuffer Engine::Synthesize(const std::string& text, const SynthesisOptions& options) {
    return impl_->Run(text, options);
}

bool Engine::SaveWav(const std::string& path, const AudioBuffer& audio, std::string* error) const {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        if (error != nullptr) {
            *error = "failed to open output file";
        }
        return false;
    }

    const std::uint16_t channels = 1;
    const std::uint16_t bits_per_sample = 16;
    const std::uint32_t byte_rate = static_cast<std::uint32_t>(audio.sample_rate) * channels * (bits_per_sample / 8);
    const std::uint16_t block_align = channels * (bits_per_sample / 8);
    const std::uint32_t data_size = static_cast<std::uint32_t>(audio.samples.size() * sizeof(std::int16_t));
    const std::uint32_t riff_size = 36u + data_size;

    stream.write("RIFF", 4);
    WriteLittleEndian(stream, riff_size, 4);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    WriteLittleEndian(stream, 16, 4);
    WriteLittleEndian(stream, 1, 2);
    WriteLittleEndian(stream, channels, 2);
    WriteLittleEndian(stream, static_cast<std::uint32_t>(audio.sample_rate), 4);
    WriteLittleEndian(stream, byte_rate, 4);
    WriteLittleEndian(stream, block_align, 2);
    WriteLittleEndian(stream, bits_per_sample, 2);
    stream.write("data", 4);
    WriteLittleEndian(stream, data_size, 4);

    for (float sample : audio.samples) {
        const float clamped = NormalizeAmplitude(sample);
        const auto pcm = static_cast<std::int16_t>(clamped * 32767.0f);
        WriteLittleEndian(stream, static_cast<std::uint16_t>(pcm), 2);
    }

    if (!stream.good()) {
        if (error != nullptr) {
            *error = "failed while writing wav data";
        }
        return false;
    }

    return true;
}

}  // namespace kokoro
