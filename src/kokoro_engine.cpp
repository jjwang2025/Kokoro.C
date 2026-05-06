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
        {"artificial", "ˌɑɹtɪfˈɪʃəl"},
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
        {"general", "dʒˈɛnəɹəl"},
        {"giuliani", "ˌdʒuliˈɑni"},
        {"goodman", "ɡˈʊdmən"},
        {"hello", "həlˈoʊ"},
        {"how", "hˌW"},
        {"i", "ˌI"},
        {"intelligence", "ɪntˈɛlɪdʒəns"},
        {"is", "ˈɪz"},
        {"kokoro", "kˈoʊkəɹoʊ"},
        {"local", "lˈoʊkəl"},
        {"monday", "mˈʌndeɪ"},
        {"on", "ˈɑːn"},
        {"reasonably", "ɹˈizənəbli"},
        {"runs", "ɹˈʌnz"},
        {"s", "z"},
        {"september", "sɛptˈɛmbɚ"},
        {"simple", "sˈɪmpəl"},
        {"test", "tˈɛst"},
        {"text", "tˈɛkst"},
        {"thank", "θˈæŋk"},
        {"this", "ðˈɪs"},
        {"today", "tədˈA"},
        {"well", "wˈɛl"},
        {"with", "wˈɪð"},
        {"world", "wˈɜːld"},
        {"xinhua", "ʃˈɪnhwɑ"},
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

void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    std::size_t start = 0;
    while ((start = text.find(from, start)) != std::string::npos) {
        text.replace(start, from.size(), to);
        start += to.size();
    }
}

std::string SmallNumberToWords(int value) {
    static const std::vector<std::string> below_twenty = {
        "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
        "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
    };
    static const std::vector<std::string> tens_words = {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety",
    };

    if (value < 20) {
        return below_twenty[value];
    }
    if (value % 10 == 0) {
        return tens_words[value / 10];
    }
    return tens_words[value / 10] + " " + below_twenty[value % 10];
}

bool IsAsciiDigitsOnly(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return false;
        }
    }
    return true;
}

std::string IntegerNumberToWords(int value) {
    if (value < 100) {
        return SmallNumberToWords(value);
    }
    if (value < 1000) {
        const int hundreds = value / 100;
        const int rest = value % 100;
        std::string result = SmallNumberToWords(hundreds) + " hundred";
        if (rest != 0) {
            result += " " + SmallNumberToWords(rest);
        }
        return result;
    }
    if (value < 1000000) {
        const int thousands = value / 1000;
        const int rest = value % 1000;
        std::string result = IntegerNumberToWords(thousands) + " thousand";
        if (rest != 0) {
            result += " " + IntegerNumberToWords(rest);
        }
        return result;
    }
    return std::to_string(value);
}

bool ParseOrdinalToken(const std::string& text, int* value) {
    if (text.size() < 3) {
        return false;
    }

    const std::string lower = ToLowerAscii(text);
    std::string suffix;
    if (lower.size() >= 2) {
        suffix = lower.substr(lower.size() - 2);
    }
    if (suffix != "st" && suffix != "nd" && suffix != "rd" && suffix != "th") {
        return false;
    }

    const std::string digits = lower.substr(0, lower.size() - 2);
    if (!IsAsciiDigitsOnly(digits)) {
        return false;
    }

    *value = std::stoi(digits);
    return true;
}

std::string IntegerOrdinalToWords(int value) {
    static const std::unordered_map<int, std::string> ordinals = {
        {1, "first"}, {2, "second"}, {3, "third"}, {4, "fourth"}, {5, "fifth"},
        {6, "sixth"}, {7, "seventh"}, {8, "eighth"}, {9, "ninth"}, {10, "tenth"},
        {11, "eleventh"}, {12, "twelfth"}, {13, "thirteenth"}, {14, "fourteenth"}, {15, "fifteenth"},
        {16, "sixteenth"}, {17, "seventeenth"}, {18, "eighteenth"}, {19, "nineteenth"}, {20, "twentieth"},
        {30, "thirtieth"}, {40, "fortieth"}, {50, "fiftieth"}, {60, "sixtieth"}, {70, "seventieth"},
        {80, "eightieth"}, {90, "ninetieth"}, {100, "hundredth"}, {1000, "thousandth"},
    };

    if (const auto it = ordinals.find(value); it != ordinals.end()) {
        return it->second;
    }

    if (value < 100) {
        const int tens = (value / 10) * 10;
        const int ones = value % 10;
        const auto tens_it = ordinals.find(tens);
        const auto ones_it = ordinals.find(ones);
        if (tens_it != ordinals.end() && ones_it != ordinals.end()) {
            return tens_it->second.substr(0, tens_it->second.size() - 2) + " " + ones_it->second;
        }
    }

    if (value < 1000) {
        const int hundreds = value / 100;
        const int rest = value % 100;
        if (rest == 0) {
            return SmallNumberToWords(hundreds) + " hundredth";
        }
        return SmallNumberToWords(hundreds) + " hundred " + IntegerOrdinalToWords(rest);
    }

    if (value < 1000000) {
        const int thousands = value / 1000;
        const int rest = value % 1000;
        if (rest == 0) {
            return IntegerNumberToWords(thousands) + " thousandth";
        }
        return IntegerNumberToWords(thousands) + " thousand " + IntegerOrdinalToWords(rest);
    }

    return IntegerNumberToWords(value);
}

bool IsLargeMoneyUnit(const std::string& text) {
    const std::string lower = ToLowerAscii(text);
    return lower == "million" || lower == "billion" || lower == "trillion";
}

std::string RemoveDigitGroupingCommas(const std::string& text) {
    std::string output;
    output.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == ',' && i > 0 && i + 1 < text.size() &&
            std::isdigit(static_cast<unsigned char>(text[i - 1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[i + 1])) != 0) {
            continue;
        }
        output.push_back(ch);
    }

    return output;
}

std::string ExpandSimpleYears(const std::string& text) {
    std::string expanded;
    expanded.reserve(text.size() + text.size() / 8);

    for (std::size_t i = 0; i < text.size();) {
        if (i + 4 <= text.size() &&
            std::isdigit(static_cast<unsigned char>(text[i])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[i + 1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[i + 2])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[i + 3])) != 0 &&
            (i == 0 || std::isalnum(static_cast<unsigned char>(text[i - 1])) == 0) &&
            (i + 4 == text.size() || std::isalnum(static_cast<unsigned char>(text[i + 4])) == 0)) {
            const int year = std::stoi(text.substr(i, 4));
            std::string spoken;

            if (year >= 1900 && year <= 1999) {
                const int tail = year % 100;
                spoken = "nineteen";
                if (tail != 0) {
                    spoken += " " + SmallNumberToWords(tail);
                } else {
                    spoken += " hundred";
                }
            } else if (year >= 2000 && year <= 2009) {
                spoken = year == 2000 ? "two thousand" : "two thousand " + SmallNumberToWords(year % 100);
            } else if (year >= 2010 && year <= 2099) {
                const int tail = year % 100;
                spoken = "twenty";
                if (tail != 0) {
                    spoken += " " + SmallNumberToWords(tail);
                }
            }

            if (!spoken.empty()) {
                expanded += spoken;
                i += 4;
                continue;
            }
        }

        expanded.push_back(text[i]);
        ++i;
    }

    return expanded;
}

std::string ExpandSimpleNumericForms(const std::string& text) {
    std::string expanded = text;

    for (int left = 0; left <= 99; ++left) {
        for (int right = 0; right <= 99; ++right) {
            const std::string slash_pattern = std::to_string(left) + "/" + std::to_string(right);
            const std::string slash_replacement = SmallNumberToWords(left) + " " + SmallNumberToWords(right);
            ReplaceAll(expanded, slash_pattern, slash_replacement);
        }
    }

    for (int value = 0; value <= 99; ++value) {
        const std::string word = SmallNumberToWords(value);

        for (int fraction = 0; fraction <= 9; ++fraction) {
            const std::string decimal_pattern = std::to_string(value) + "." + std::to_string(fraction);
            const std::string decimal_replacement = word + " point " + SmallNumberToWords(fraction);
            ReplaceAll(expanded, decimal_pattern, decimal_replacement);
        }

        const std::string percent_pattern = std::to_string(value) + "%";
        ReplaceAll(expanded, percent_pattern, word + " percent");
    }

    return expanded;
}

std::string ExpandSimpleNewsDates(const std::string& text) {
    static const std::unordered_map<int, std::string> ordinals = {
        {1, "first"},       {2, "second"},      {3, "third"},        {4, "fourth"},
        {5, "fifth"},       {6, "sixth"},       {7, "seventh"},      {8, "eighth"},
        {9, "ninth"},       {10, "tenth"},      {11, "eleventh"},    {12, "twelfth"},
        {13, "thirteenth"}, {14, "fourteenth"}, {15, "fifteenth"},   {16, "sixteenth"},
        {17, "seventeenth"},{18, "eighteenth"}, {19, "nineteenth"},  {20, "twentieth"},
        {21, "twenty first"},{22, "twenty second"},{23, "twenty third"},{24, "twenty fourth"},
        {25, "twenty fifth"},{26, "twenty sixth"}, {27, "twenty seventh"},{28, "twenty eighth"},
        {29, "twenty ninth"},{30, "thirtieth"}, {31, "thirty first"},
    };

    static const std::vector<std::string> months = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december",
    };

    std::string expanded = text;
    for (const std::string& month : months) {
        for (int day = 1; day <= 31; ++day) {
            const auto ordinal_it = ordinals.find(day);
            if (ordinal_it == ordinals.end()) {
                continue;
            }

            for (int end_day = 1; end_day <= 31; ++end_day) {
                const auto end_ordinal_it = ordinals.find(end_day);
                if (end_ordinal_it == ordinals.end()) {
                    continue;
                }

                const std::string range_pattern = month + " " + std::to_string(day) + " to " + std::to_string(end_day);
                const std::string range_replacement = month + " " + ordinal_it->second + " to " + end_ordinal_it->second;
                ReplaceAll(expanded, range_pattern, range_replacement);
            }

            const std::string numeric_pattern = month + " " + std::to_string(day) + ",";
            const std::string numeric_replacement = month + " " + ordinal_it->second + ",";

            ReplaceAll(expanded, numeric_pattern, numeric_replacement);

            const std::string compact_pattern = month + " " + std::to_string(day) + " ,";
            const std::string compact_replacement = month + " " + ordinal_it->second + " ,";
            ReplaceAll(expanded, compact_pattern, compact_replacement);

            const std::string bare_pattern = month + " " + std::to_string(day) + " ";
            const std::string bare_replacement = month + " " + ordinal_it->second + " ";
            ReplaceAll(expanded, bare_pattern, bare_replacement);

            const std::string closing_pattern = month + " " + std::to_string(day) + ")";
            const std::string closing_replacement = month + " " + ordinal_it->second + ")";
            ReplaceAll(expanded, closing_pattern, closing_replacement);
        }
    }
    return expanded;
}

std::string NormalizeEnglishText(const std::string& text) {
    std::string normalized = text;
    normalized = std::regex_replace(normalized, std::regex("[‘’]"), "'");
    normalized = std::regex_replace(normalized, std::regex("[“”]"), "\"");
    normalized = std::regex_replace(normalized, std::regex("(^|[\\s\\[(])'(?=[A-Za-z])"), "$1, ");
    normalized = std::regex_replace(normalized, std::regex("([A-Za-z])'([\\s\\],.!?;:])"), "$1,$2");
    normalized = std::regex_replace(normalized, std::regex("([A-Za-z])'$"), "$1,");
    normalized = std::regex_replace(normalized, std::regex("\\b[Dd][Rr]\\.(?= [A-Z])"), "Doctor");
    normalized = std::regex_replace(normalized, std::regex("\\b(?:Mr\\.|MR\\.(?= [A-Z]))"), "Mister");
    normalized = std::regex_replace(normalized, std::regex("\\b(?:Ms\\.|MS\\.(?= [A-Z]))"), "Miss");
    normalized = std::regex_replace(normalized, std::regex("\\b(?:Mrs\\.|MRS\\.(?= [A-Z]))"), "Mrs");
    normalized = std::regex_replace(normalized, std::regex("\\bSept\\.", std::regex::icase), "September");
    normalized = std::regex_replace(normalized, std::regex("\\bJan\\.", std::regex::icase), "January");
    normalized = std::regex_replace(normalized, std::regex("\\bFeb\\.", std::regex::icase), "February");
    normalized = std::regex_replace(normalized, std::regex("\\bMar\\.", std::regex::icase), "March");
    normalized = std::regex_replace(normalized, std::regex("\\bApr\\.", std::regex::icase), "April");
    normalized = std::regex_replace(normalized, std::regex("\\bJun\\.", std::regex::icase), "June");
    normalized = std::regex_replace(normalized, std::regex("\\bJul\\.", std::regex::icase), "July");
    normalized = std::regex_replace(normalized, std::regex("\\bAug\\.", std::regex::icase), "August");
    normalized = std::regex_replace(normalized, std::regex("\\bOct\\.", std::regex::icase), "October");
    normalized = std::regex_replace(normalized, std::regex("\\bNov\\.", std::regex::icase), "November");
    normalized = std::regex_replace(normalized, std::regex("\\bDec\\.", std::regex::icase), "December");
    normalized = std::regex_replace(normalized, std::regex("\\betc\\.(?! [A-Z])", std::regex::icase), "etc");
    normalized = ToLowerAscii(normalized);
    normalized = std::regex_replace(normalized, std::regex("[()]"), ", ");
    normalized = ExpandSimpleNumericForms(normalized);
    normalized = ExpandSimpleNewsDates(normalized);
    normalized = ExpandSimpleYears(normalized);
    normalized = RemoveDigitGroupingCommas(normalized);
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
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '\'';
}

bool HasMixedCase(const std::string& token) {
    bool has_upper = false;
    bool has_lower = false;
    for (char ch : token) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isupper(uch) != 0) {
            has_upper = true;
        } else if (std::islower(uch) != 0) {
            has_lower = true;
        }
    }
    return has_upper && has_lower;
}

bool IsUppercaseAlphaWord(const std::string& token) {
    if (token.size() < 2) {
        return false;
    }
    for (char ch : token) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalpha(uch) == 0 || std::isupper(uch) == 0) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> SplitMixedCaseToken(const std::string& token) {
    std::vector<std::string> parts;
    std::string current;

    auto is_upper = [](char ch) {
        return std::isupper(static_cast<unsigned char>(ch)) != 0;
    };
    auto is_lower = [](char ch) {
        return std::islower(static_cast<unsigned char>(ch)) != 0;
    };
    auto is_alpha = [](char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) != 0;
    };

    for (std::size_t i = 0; i < token.size(); ++i) {
        const char current_char = token[i];
        if (i > 0) {
            const char previous = token[i - 1];
            const char next = (i + 1 < token.size()) ? token[i + 1] : '\0';

            const bool lower_to_upper = is_lower(previous) && is_upper(current_char);
            const bool acronym_to_word = is_upper(previous) && is_upper(current_char) && next != '\0' && is_lower(next);
            const bool digit_to_alpha = std::isdigit(static_cast<unsigned char>(previous)) != 0 && is_alpha(current_char);
            const bool alpha_to_digit = is_alpha(previous) && std::isdigit(static_cast<unsigned char>(current_char)) != 0;

            if (lower_to_upper || acronym_to_word || digit_to_alpha || alpha_to_digit) {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            }
        }
        current.push_back(current_char);
    }

    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::vector<std::string> ExpandTokenVariants(const std::string& token) {
    if (token.empty()) {
        return {};
    }

    if (IsUppercaseAlphaWord(token) && token.size() <= 4) {
        std::vector<std::string> parts;
        parts.reserve(token.size());
        for (char ch : token) {
            parts.emplace_back(1, static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return parts;
    }

    if (!HasMixedCase(token)) {
        return {token};
    }

    const auto parts = SplitMixedCaseToken(token);
    if (parts.size() < 2) {
        return {token};
    }

    std::vector<std::string> result;
    for (const std::string& part : parts) {
        if (IsUppercaseAlphaWord(part) && part.size() <= 4) {
            for (char ch : part) {
                result.emplace_back(1, static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        } else {
            result.push_back(part);
        }
    }
    return result;
}

bool StartsWithVowelLikeSound(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    const std::string lower = ToLowerAscii(token);
    static const std::unordered_map<std::string, bool> special_cases = {
        {"honest", true},   {"honor", true},     {"hour", true},        {"heir", true},
        {"mba", true},      {"xml", true},       {"fbi", true},         {"api", true},
        {"one", false},     {"once", false},     {"user", false},       {"university", false},
        {"european", false}, {"unicorn", false}, {"ubiquitous", false}, {"url", false},
    };

    if (const auto it = special_cases.find(lower); it != special_cases.end()) {
        return it->second;
    }

    const char ch = lower.front();
    return ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u';
}

std::string WeakFormForToken(const std::string& lower, const std::string* next_word) {
    const bool next_is_vowel_like = next_word != nullptr && StartsWithVowelLikeSound(*next_word);

    if (lower == "the") {
        return next_is_vowel_like ? "ði" : "ðə";
    }
    if (lower == "a") {
        return next_is_vowel_like ? "eɪ" : "ə";
    }
    if (lower == "an") {
        return "ən";
    }
    if (lower == "to") {
        return next_is_vowel_like ? "tu" : "tə";
    }
    if (lower == "of") {
        return "əv";
    }
    if (lower == "and") {
        return "ənd";
    }
    if (lower == "for") {
        return "fɚ";
    }
    if (lower == "our") {
        return "aʊɚ";
    }
    if (lower == "can") {
        return "kən";
    }
    if (lower == "some") {
        return "səm";
    }
    if (lower == "from") {
        return "fɹəm";
    }
    if (lower == "was") {
        return "wəz";
    }
    if (lower == "were") {
        return "wɚ";
    }
    return {};
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

std::vector<std::string> SplitPhonemeClauses(const std::string& phonemes) {
    std::vector<std::string> clauses;
    std::string current;

    auto flush = [&clauses, &current]() {
        const std::string trimmed = Trim(current);
        if (!trimmed.empty()) {
            clauses.push_back(trimmed);
        }
        current.clear();
    };

    for (char ch : phonemes) {
        current.push_back(ch);
        if (ch == '.' || ch == '!' || ch == '?' || ch == ',' || ch == ';' || ch == ':') {
            flush();
        }
    }
    flush();
    return clauses;
}

std::string FallbackWordToPhonemes(const std::string& word) {
    std::string lower = ToLowerAscii(word);

    if (lower.size() == 1 && std::isalpha(static_cast<unsigned char>(lower.front())) != 0) {
        static const std::unordered_map<char, std::string> letter_names = {
            {'a', "eɪ"}, {'b', "bi"}, {'c', "si"}, {'d', "di"}, {'e', "i"}, {'f', "ɛf"},
            {'g', "dʒi"}, {'h', "eɪtʃ"}, {'i', "aɪ"}, {'j', "dʒeɪ"}, {'k', "keɪ"}, {'l', "ɛl"},
            {'m', "ɛm"}, {'n', "ɛn"}, {'o', "oʊ"}, {'p', "pi"}, {'q', "kju"}, {'r', "ɑɹ"},
            {'s', "ɛs"}, {'t', "ti"}, {'u', "ju"}, {'v', "vi"}, {'w', "dʌbəlju"}, {'x', "ɛks"},
            {'y', "waɪ"}, {'z', "zi"},
        };
        const auto it = letter_names.find(lower.front());
        if (it != letter_names.end()) {
            return it->second;
        }
    }

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
    return phonemes;
}

std::string PossessiveSuffixForPhonemes(const std::string& phonemes) {
    const std::string trimmed = Trim(phonemes);
    if (trimmed.empty()) {
        return {};
    }

    auto ends_with = [&trimmed](const std::string& suffix) {
        return trimmed.size() >= suffix.size() && trimmed.compare(trimmed.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    if (ends_with("s") || ends_with("z") || ends_with("ʃ") || ends_with("ʒ") || ends_with("tʃ") || ends_with("dʒ")) {
        return "ɪz";
    }

    if (ends_with("p") || ends_with("t") || ends_with("k") || ends_with("f") || ends_with("θ")) {
        return "s";
    }

    return "z";
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

float ClampEmotionStrength(float strength) {
    if (strength < 0.0f) {
        return 0.0f;
    }
    if (strength > 1.0f) {
        return 1.0f;
    }
    return strength;
}

float ApplyEmotionSpeed(float speed, EmotionPreset emotion, float emotion_strength) {
    if (emotion == EmotionPreset::Happy) {
        return ClampSpeed(speed * (1.0f + 0.12f * ClampEmotionStrength(emotion_strength)));
    }
    return ClampSpeed(speed);
}

std::string ApplyEmotionPunctuation(const std::string& token, EmotionPreset emotion, float emotion_strength) {
    if (emotion != EmotionPreset::Happy || ClampEmotionStrength(emotion_strength) <= 0.0f) {
        return token;
    }
    if (token == ";" || token == ":") {
        return ",";
    }
    return token;
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

std::string ResolveDefaultLexiconPath(const std::string& path, const std::string& fallback) {
    if (!path.empty()) {
        return path;
    }
    return FileExists(fallback) ? fallback : std::string();
}

std::string NormalizeDictionaryKey(std::string word) {
    const std::size_t pronunciation_suffix = word.find('(');
    if (pronunciation_suffix != std::string::npos) {
        word = word.substr(0, pronunciation_suffix);
    }
    return ToLowerAscii(Trim(word));
}

std::vector<std::string> ParseDictionaryPhones(const std::string& text) {
    std::vector<std::string> phones;
    std::stringstream stream(text);
    std::string item;
    while (stream >> item) {
        item = Trim(item);
        if (!item.empty() && item != "-") {
            phones.push_back(item);
        }
    }
    return phones;
}

std::vector<std::string> ParseCachedDictionaryPhones(const std::string& text) {
    std::vector<std::string> phones;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t quote_begin = text.find('\'', pos);
        if (quote_begin == std::string::npos) {
            break;
        }
        const std::size_t quote_end = text.find('\'', quote_begin + 1);
        if (quote_end == std::string::npos) {
            break;
        }

        const std::string token = Trim(text.substr(quote_begin + 1, quote_end - quote_begin - 1));
        if (!token.empty()) {
            phones.push_back(token);
        }
        pos = quote_end + 1;
    }
    return phones;
}

std::string ArpaPhoneToKokoroPhoneme(const std::string& phone) {
    int stress = 0;
    std::string base = phone;
    if (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back())) != 0) {
        stress = base.back() - '0';
        base.pop_back();
    }

    const std::string normalized = ToLowerAscii(base);

    if (normalized == "ah") {
        if (stress == 1) {
            return "ˈʌ";
        }
        if (stress == 2) {
            return "ˌʌ";
        }
        return "ə";
    }

    if (normalized == "er") {
        if (stress == 1) {
            return "ˈɚ";
        }
        if (stress == 2) {
            return "ˌɚ";
        }
        return "ɚ";
    }

    if (normalized == "ih") {
        if (stress == 0) {
            return "ə";
        }
        return stress == 1 ? "ˈɪ" : "ˌɪ";
    }

    if (normalized == "uh") {
        if (stress == 0) {
            return "ə";
        }
        return stress == 1 ? "ˈʊ" : "ˌʊ";
    }

    if (normalized == "aa") {
        return stress == 1 ? "ˈɑ" : (stress == 2 ? "ˌɑ" : "ɑ");
    }

    if (normalized == "ao") {
        return stress == 1 ? "ˈɔ" : (stress == 2 ? "ˌɔ" : "ɔ");
    }

    if (normalized == "uw") {
        return stress == 1 ? "ˈu" : (stress == 2 ? "ˌu" : "u");
    }

    if (normalized == "iy") {
        return stress == 1 ? "ˈi" : (stress == 2 ? "ˌi" : "i");
    }

    static const std::unordered_map<std::string, std::string> arpa_to_ipa = {
        {"aa", "ɑ"}, {"ae", "æ"}, {"ao", "ɔ"}, {"aw", "aʊ"},
        {"ay", "aɪ"}, {"b", "b"}, {"ch", "tʃ"}, {"d", "d"}, {"dh", "ð"},
        {"eh", "ɛ"}, {"ey", "eɪ"}, {"f", "f"}, {"g", "ɡ"},
        {"hh", "h"}, {"jh", "dʒ"}, {"k", "k"},
        {"l", "l"}, {"m", "m"}, {"n", "n"}, {"ng", "ŋ"}, {"ow", "oʊ"},
        {"oy", "ɔɪ"}, {"p", "p"}, {"r", "ɹ"}, {"s", "s"}, {"sh", "ʃ"},
        {"t", "t"}, {"th", "θ"}, {"v", "v"},
        {"w", "w"}, {"y", "j"}, {"z", "z"}, {"zh", "ʒ"},
    };

    const auto it = arpa_to_ipa.find(normalized);
    if (it == arpa_to_ipa.end()) {
        return {};
    }

    std::string mapped = it->second;
    if (stress == 1) {
        mapped = "ˈ" + mapped;
    } else if (stress == 2) {
        mapped = "ˌ" + mapped;
    }
    return mapped;
}

std::string DictionaryPhonesToKokoroPhonemes(const std::vector<std::string>& phones) {
    std::string output;
    for (const std::string& phone : phones) {
        const std::string mapped = ArpaPhoneToKokoroPhoneme(phone);
        if (mapped.empty()) {
            continue;
        }
        output += mapped;
    }
    return output;
}

class DictionaryLexicon {
public:
    DictionaryLexicon() = default;

    explicit DictionaryLexicon(const std::string& path) {
        if (path.empty()) {
            return;
        }

        std::ifstream input(path);
        if (!input) {
            return;
        }

        std::string line;
        while (std::getline(input, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::string word;
            std::vector<std::string> phones;
            const std::size_t cache_divider = line.find(':');
            if (cache_divider != std::string::npos) {
                word = NormalizeDictionaryKey(line.substr(0, cache_divider));
                phones = ParseCachedDictionaryPhones(line.substr(cache_divider + 1));
            } else {
                const std::size_t tab_pos = line.find('\t');
                const std::size_t split_pos = tab_pos != std::string::npos ? tab_pos : line.find("  ");
                if (split_pos == std::string::npos) {
                    const std::size_t first_space = line.find(' ');
                    if (first_space == std::string::npos) {
                        continue;
                    }
                    word = NormalizeDictionaryKey(line.substr(0, first_space));
                    phones = ParseDictionaryPhones(line.substr(first_space + 1));
                } else {
                    word = NormalizeDictionaryKey(line.substr(0, split_pos));
                    phones = ParseDictionaryPhones(line.substr(split_pos + 1));
                }
            }

            if (!word.empty() && !phones.empty() && entries_.count(word) == 0) {
                entries_[word] = std::move(phones);
            }
        }
    }

    const std::vector<std::string>* Lookup(const std::string& word) const {
        const auto it = entries_.find(ToLowerAscii(word));
        return it == entries_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> entries_;
};

std::string PhonemizeEnglishText(const std::string& text,
                                 const DictionaryLexicon* g2p_lexicon = nullptr,
                                 const DictionaryLexicon* cmudict = nullptr,
                                 EmotionPreset emotion = EmotionPreset::Neutral,
                                 float emotion_strength = 0.0f) {
    const std::string normalized = NormalizeEnglishText(text);
    const auto parts = SplitWordsKeepingPunctuation(normalized);
    const auto& lexicon = EnglishLexicon();

    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::string& part = parts[i];
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
            result += ApplyEmotionPunctuation(MapPunctuationPhone(part), emotion, emotion_strength);
            continue;
        }

        if (part == "$" && i + 1 < parts.size()) {
            const std::string amount_token = parts[i + 1];
            if (IsAsciiDigitsOnly(amount_token)) {
                std::string money_phrase = IntegerNumberToWords(std::stoi(amount_token));
                std::size_t consumed = 1;

                std::size_t next_index = i + 2;
                while (next_index < parts.size() && parts[next_index] == " ") {
                    ++next_index;
                }

                if (next_index < parts.size() && IsLargeMoneyUnit(parts[next_index])) {
                    money_phrase += " " + ToLowerAscii(parts[next_index]);
                    consumed = next_index - i;
                }

                money_phrase += " dollars";
                const std::string expanded_phonemes = PhonemizeEnglishText(money_phrase, g2p_lexicon, cmudict, emotion, emotion_strength);
                if (!expanded_phonemes.empty()) {
                    result += expanded_phonemes;
                    result.push_back(' ');
                    i += consumed;
                    continue;
                }
            }
        }

        const auto expanded_parts = ExpandTokenVariants(part);
        if (expanded_parts.size() > 1 || (expanded_parts.size() == 1 && expanded_parts.front() != part)) {
            for (const std::string& expanded : expanded_parts) {
                std::string expanded_phonemes = PhonemizeEnglishText(expanded, g2p_lexicon, cmudict, emotion, emotion_strength);
                if (!expanded_phonemes.empty()) {
                    if (!result.empty() && result.back() != ' ') {
                        result.push_back(' ');
                    }
                    result += expanded_phonemes;
                    result.push_back(' ');
                }
            }
            continue;
        }

        const std::string lower = ToLowerAscii(part);

        if (lower.size() > 2 && lower.substr(lower.size() - 2) == "'s") {
            const std::string stem = part.substr(0, part.size() - 2);
            const std::string stem_phonemes = PhonemizeEnglishText(stem, g2p_lexicon, cmudict, emotion, emotion_strength);
            const std::string suffix = PossessiveSuffixForPhonemes(stem_phonemes);
            if (!stem_phonemes.empty() && !suffix.empty()) {
                result += stem_phonemes + suffix;
                result.push_back(' ');
                continue;
            }
        }

        if (IsAsciiDigitsOnly(lower)) {
            const int numeric_value = std::stoi(lower);
            const std::string spoken_number = IntegerNumberToWords(numeric_value);
            if (!spoken_number.empty() && spoken_number != lower) {
                const std::string expanded_phonemes = PhonemizeEnglishText(spoken_number, g2p_lexicon, cmudict, emotion, emotion_strength);
                if (!expanded_phonemes.empty()) {
                    result += expanded_phonemes;
                    result.push_back(' ');
                    continue;
                }
            }
        }

        int ordinal_value = 0;
        if (ParseOrdinalToken(lower, &ordinal_value)) {
            const std::string spoken_ordinal = IntegerOrdinalToWords(ordinal_value);
            if (!spoken_ordinal.empty()) {
                const std::string expanded_phonemes = PhonemizeEnglishText(spoken_ordinal, g2p_lexicon, cmudict, emotion, emotion_strength);
                if (!expanded_phonemes.empty()) {
                    result += expanded_phonemes;
                    result.push_back(' ');
                    continue;
                }
            }
        }

        const std::string* next_word = nullptr;
        for (std::size_t next = i + 1; next < parts.size(); ++next) {
            if (parts[next].empty() || parts[next] == " " || IsPunctuationToken(parts[next])) {
                continue;
            }
            next_word = &parts[next];
            break;
        }

        const auto builtin = lexicon.find(lower);
        if (builtin != lexicon.end()) {
            result += builtin->second;
            result.push_back(' ');
            continue;
        }

        if (g2p_lexicon != nullptr) {
            if (const auto* entry = g2p_lexicon->Lookup(part)) {
                const std::string mapped = DictionaryPhonesToKokoroPhonemes(*entry);
                if (!mapped.empty()) {
                    result += mapped;
                    result.push_back(' ');
                    continue;
                }
            }
        }

        if (cmudict != nullptr) {
            if (const auto* entry = cmudict->Lookup(part)) {
                const std::string mapped = DictionaryPhonesToKokoroPhonemes(*entry);
                if (!mapped.empty()) {
                    result += mapped;
                    result.push_back(' ');
                    continue;
                }
            }
        }

        const std::string weak_form = WeakFormForToken(lower, next_word);
        if (!weak_form.empty()) {
            result += weak_form;
            result.push_back(' ');
            continue;
        }

        const std::string fallback = FallbackWordToPhonemes(part);
        if (!fallback.empty()) {
            result += fallback;
            result.push_back(' ');
        }
    }

    result = std::regex_replace(result, std::regex("k ə k ˈo ʊ k ə ɹ o ʊ"), "kˈoʊkəɹoʊ");
    result = CollapseSpacesAroundPunctuation(result);
    result = std::regex_replace(result, std::regex("r"), "ɹ");
    return result;
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
        const std::string resolved_cmudict_path = ResolveDefaultLexiconPath(options.cmudict_path, "lexicons/cmudict_cache_upper.txt");
        const std::string resolved_g2p_lexicon_path = ResolveDefaultLexiconPath(options.g2p_lexicon_path, "lexicons/programming_terms.lexicon");
        if (loaded_model_path == options.model_path &&
            loaded_tokenizer_path == options.tokenizer_path &&
            loaded_voice_path == resolved_voice_path &&
            loaded_cmudict_path == resolved_cmudict_path &&
            loaded_g2p_lexicon_path == resolved_g2p_lexicon_path &&
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
        cmudict = DictionaryLexicon(resolved_cmudict_path);
        g2p_lexicon = DictionaryLexicon(resolved_g2p_lexicon_path);

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
        loaded_cmudict_path = resolved_cmudict_path;
        loaded_g2p_lexicon_path = resolved_g2p_lexicon_path;
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
                stream << "tokenizer vocab does not contain codepoint in phoneme input: " << codepoint << " [bytes=";
                for (std::size_t i = 0; i < codepoint.size(); ++i) {
                    if (i > 0) {
                        stream << ' ';
                    }
                    stream << std::hex << std::uppercase
                           << static_cast<int>(static_cast<unsigned char>(codepoint[i]));
                }
                stream << std::dec << "]";
                throw std::runtime_error(stream.str());
            }
        }

        input_ids.push_back(0);
        if (input_ids.size() > 512) {
            throw std::runtime_error("phoneme input exceeds Kokoro context length");
        }
        return input_ids;
    }

    std::vector<float> SelectStyle(const std::vector<std::int64_t>& input_ids,
                                   EmotionPreset emotion,
                                   float emotion_strength) const {
        const std::size_t phoneme_count = input_ids.size() >= 2 ? (input_ids.size() - 2) : 0;
        std::size_t style_index = std::min<std::size_t>(phoneme_count, 509);
        if (emotion == EmotionPreset::Happy) {
            const std::size_t offset = static_cast<std::size_t>(std::round(4.0f * ClampEmotionStrength(emotion_strength)));
            style_index = std::min<std::size_t>(style_index + offset, 509);
        }
        const std::size_t offset = style_index * kStyleDim;

        if (offset + kStyleDim > voice_data.size()) {
            throw std::runtime_error("voice style index out of range");
        }

        return std::vector<float>(voice_data.begin() + static_cast<std::ptrdiff_t>(offset), voice_data.begin() + static_cast<std::ptrdiff_t>(offset + kStyleDim));
    }

    AudioBuffer RunPhonemes(const std::string& phonemes, const SynthesisOptions& options) {
        std::vector<std::int64_t> input_ids = TokenizePhonemes(phonemes);
        std::vector<float> style = SelectStyle(input_ids, options.emotion, options.emotion_strength);
        float speed = ApplyEmotionSpeed(options.speed, options.emotion, options.emotion_strength);

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

    AudioBuffer Run(const std::string& text, const SynthesisOptions& options) {
        EnsureLoaded(options);

        const std::string phonemes = options.input_is_phonemes ? text : PhonemizeEnglishText(text, &g2p_lexicon, &cmudict, options.emotion, options.emotion_strength);
        AudioBuffer audio;
        audio.sample_rate = kSampleRate;

        const auto clauses = SplitPhonemeClauses(phonemes);
        std::string current_chunk;

        auto append_chunk = [this, &audio, &options](const std::string& chunk) {
            if (chunk.empty()) {
                return;
            }
            AudioBuffer part = RunPhonemes(chunk, options);
            audio.samples.insert(audio.samples.end(), part.samples.begin(), part.samples.end());
        };

        for (const std::string& clause : clauses) {
            std::string candidate = current_chunk.empty() ? clause : current_chunk + " " + clause;
            try {
                (void)TokenizePhonemes(candidate);
                current_chunk = std::move(candidate);
                continue;
            } catch (const std::runtime_error& ex) {
                if (std::string(ex.what()) != "phoneme input exceeds Kokoro context length") {
                    throw;
                }
            }

            if (current_chunk.empty()) {
                throw std::runtime_error("single clause exceeds Kokoro context length");
            }

            append_chunk(current_chunk);
            current_chunk = clause;
        }

        append_chunk(current_chunk);
        return audio;
    }

private:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    std::vector<std::pair<std::string, std::int64_t>> tokenizer_vocab;
    std::vector<float> voice_data;
    DictionaryLexicon cmudict;
    DictionaryLexicon g2p_lexicon;
    std::string loaded_model_path;
    std::string loaded_tokenizer_path;
    std::string loaded_voice_path;
    std::string loaded_cmudict_path;
    std::string loaded_g2p_lexicon_path;
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
