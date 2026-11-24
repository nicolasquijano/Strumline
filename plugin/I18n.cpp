#include "I18n.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace i18n {

const std::unordered_map<std::string, std::string>& spanishTable();     // I18nEs.cpp
const std::unordered_map<std::string, std::string>& portugueseTable();  // I18nPt.cpp
const std::unordered_map<std::string, std::string>& italianTable();     // I18nIt.cpp
const std::unordered_map<std::string, std::string>& chineseTable();     // I18nZh.cpp
const std::unordered_map<std::string, std::string>& frenchTable();      // I18nFr.cpp
const std::unordered_map<std::string, std::string>& germanTable();      // I18nDe.cpp
const std::unordered_map<std::string, std::string>& japaneseTable();    // I18nJa.cpp
const std::unordered_map<std::string, std::string>& russianTable();     // I18nRu.cpp
const std::unordered_map<std::string, std::string>& koreanTable();      // I18nKo.cpp
const std::unordered_map<std::string, std::string>& traditionalChineseTable();  // I18nZhTw.cpp
const std::unordered_map<std::string, std::string>& turkishTable();     // I18nTr.cpp
const std::unordered_map<std::string, std::string>& polishTable();      // I18nPl.cpp
const std::unordered_map<std::string, std::string>& dutchTable();       // I18nNl.cpp
const std::unordered_map<std::string, std::string>& swedishTable();     // I18nSv.cpp
const std::unordered_map<std::string, std::string>& indonesianTable();  // I18nId.cpp
const std::unordered_map<std::string, std::string>& vietnameseTable();  // I18nVi.cpp
const std::unordered_map<std::string, std::string>& ukrainianTable();   // I18nUk.cpp
const std::unordered_map<std::string, std::string>& finnishTable();     // I18nFi.cpp
const std::unordered_map<std::string, std::string>& catalanTable();     // I18nCa.cpp
const std::unordered_map<std::string, std::string>& hindiTable();       // I18nHi.cpp

namespace {
std::mutex gMutex;
bool gLoaded = false;
Language gPreference = Language::Auto;
Language gActive = Language::English;

juce::File settingsFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Strumline").getChildFile("language.txt");
}

// What the settings file says for a language.
juce::String languageKey(Language lang) {
    switch (lang) {
        case Language::English: return "english";
        case Language::Spanish: return "spanish";
        case Language::Portuguese: return "portuguese";
        case Language::Italian: return "italian";
        case Language::Chinese: return "chinese";
        case Language::French: return "french";
        case Language::German: return "german";
        case Language::Japanese: return "japanese";
        case Language::Russian: return "russian";
        case Language::Korean: return "korean";
        case Language::TraditionalChinese: return "traditionalchinese";
        case Language::Turkish: return "turkish";
        case Language::Polish: return "polish";
        case Language::Dutch: return "dutch";
        case Language::Swedish: return "swedish";
        case Language::Indonesian: return "indonesian";
        case Language::Vietnamese: return "vietnamese";
        case Language::Ukrainian: return "ukrainian";
        case Language::Finnish: return "finnish";
        case Language::Catalan: return "catalan";
        case Language::Hindi: return "hindi";
        default: return "auto";
    }
}

Language resolve(Language pref) {
    if (pref == Language::Auto) return fromSystemCode(juce::SystemStats::getUserLanguage());
    return pref;
}

void ensureLoaded() {
    if (gLoaded) return;
    gLoaded = true;
    const auto text = settingsFile().loadFileAsString().trim().toLowerCase();
    gPreference = Language::Auto;
    for (auto lang : {Language::English, Language::Spanish, Language::Portuguese, Language::Italian, Language::Chinese, Language::French, Language::German, Language::Japanese, Language::Russian, Language::Korean,
                      Language::TraditionalChinese, Language::Turkish, Language::Polish, Language::Dutch,
                      Language::Swedish, Language::Indonesian, Language::Vietnamese, Language::Ukrainian, Language::Finnish, Language::Catalan, Language::Hindi})
        if (text == languageKey(lang)) gPreference = lang;
    gActive = resolve(gPreference);
}
}  // namespace

Language fromSystemCode(const juce::String& code) {
    const auto c = code.trim().toLowerCase();
    if (c.startsWith("es")) return Language::Spanish;
    if (c.startsWith("pt")) return Language::Portuguese;
    if (c.startsWith("it")) return Language::Italian;
    if (c.startsWith("zh")) {
        // Traditional script: Taiwan, Hong Kong, Macao, or an explicit Hant tag; everything else Chinese is simplified.
        const bool traditional = c.contains("tw") || c.contains("hk") || c.contains("mo") || c.contains("hant");
        return traditional ? Language::TraditionalChinese : Language::Chinese;
    }
    if (c.startsWith("fr")) return Language::French;
    if (c.startsWith("de")) return Language::German;
    if (c.startsWith("ja")) return Language::Japanese;
    if (c.startsWith("ru")) return Language::Russian;
    if (c.startsWith("ko")) return Language::Korean;
    if (c.startsWith("tr")) return Language::Turkish;
    if (c.startsWith("pl")) return Language::Polish;
    if (c.startsWith("nl")) return Language::Dutch;
    if (c.startsWith("sv")) return Language::Swedish;
    if (c.startsWith("id")) return Language::Indonesian;
    if (c.startsWith("vi")) return Language::Vietnamese;
    if (c.startsWith("uk")) return Language::Ukrainian;
    if (c.startsWith("fi")) return Language::Finnish;
    if (c.startsWith("ca")) return Language::Catalan;
    if (c.startsWith("hi")) return Language::Hindi;
    return Language::English;
}

Language preference() {
    std::lock_guard<std::mutex> lock(gMutex);
    ensureLoaded();
    return gPreference;
}

void setPreference(Language lang) {
    std::lock_guard<std::mutex> lock(gMutex);
    ensureLoaded();
    gPreference = lang;
    gActive = resolve(lang);
    const auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(languageKey(lang));
}

void useForSession(Language lang) {
    std::lock_guard<std::mutex> lock(gMutex);
    ensureLoaded();
    gActive = lang == Language::Auto ? fromSystemCode(juce::SystemStats::getUserLanguage()) : lang;
}

Language active() {
    std::lock_guard<std::mutex> lock(gMutex);
    ensureLoaded();
    return gActive;
}

juce::String languageName(Language lang) {
    switch (lang) {
        case Language::English: return "English";
        case Language::Spanish: return juce::String::fromUTF8("Espa\xc3\xb1ol");
        case Language::Portuguese: return juce::String::fromUTF8("Portugu\xc3\xaas");
        case Language::Italian: return "Italiano";
        case Language::Chinese: return juce::String::fromUTF8("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87");  // 简体中文
        case Language::French: return juce::String::fromUTF8("Fran\xc3\xa7" "ais");
        case Language::German: return "Deutsch";
        case Language::Japanese: return juce::String::fromUTF8("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");  // 日本語
        case Language::Russian: return juce::String::fromUTF8("\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9");  // Русский
        case Language::Korean: return juce::String::fromUTF8("\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4");  // 한국어
        case Language::TraditionalChinese: return juce::String::fromUTF8("\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87");  // 繁體中文
        case Language::Turkish: return juce::String::fromUTF8("T\xc3\xbcrk\xc3\xa7" "e");  // Türkçe
        case Language::Polish: return "Polski";
        case Language::Dutch: return "Nederlands";
        case Language::Swedish: return "Svenska";
        case Language::Indonesian: return "Bahasa Indonesia";
        case Language::Vietnamese: return juce::String::fromUTF8("Ti\xe1\xba\xbf" "ng Vi\xe1\xbb\x87" "t");  // Tiếng Việt
        case Language::Ukrainian: return juce::String::fromUTF8("\xd0\xa3\xd0\xba\xd1\x80\xd0\xb0\xd1\x97\xd0\xbd\xd1\x81\xd1\x8c\xd0\xba\xd0\xb0");  // Українська
        case Language::Finnish: return "Suomi";
        case Language::Catalan: return juce::String::fromUTF8("Catal\xc3\xa0");
        case Language::Hindi: return juce::String::fromUTF8("\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\xa8\xe0\xa5\x8d\xe0\xa4\xa6\xe0\xa5\x80");  // हिन्दी
        default: return "Auto";
    }
}

juce::String lookup(const juce::String& english) {
    if (english.isEmpty()) return english;
    Language lang;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        ensureLoaded();
        lang = gActive;
    }
    if (lang == Language::English || lang == Language::Auto) return english;
    const auto& table = lang == Language::Spanish      ? spanishTable()
                        : lang == Language::Portuguese ? portugueseTable()
                        : lang == Language::Italian    ? italianTable()
                        : lang == Language::Chinese    ? chineseTable()
                        : lang == Language::French     ? frenchTable()
                        : lang == Language::German     ? germanTable()
                        : lang == Language::Japanese   ? japaneseTable()
                        : lang == Language::Russian    ? russianTable()
                        : lang == Language::Korean     ? koreanTable()
                        : lang == Language::TraditionalChinese ? traditionalChineseTable()
                        : lang == Language::Turkish    ? turkishTable()
                        : lang == Language::Polish     ? polishTable()
                        : lang == Language::Dutch      ? dutchTable()
                        : lang == Language::Swedish    ? swedishTable()
                        : lang == Language::Indonesian ? indonesianTable()
                        : lang == Language::Vietnamese ? vietnameseTable()
                        : lang == Language::Ukrainian  ? ukrainianTable()
                        : lang == Language::Finnish    ? finnishTable()
                        : lang == Language::Catalan    ? catalanTable()
                                                       : hindiTable();
    const auto it = table.find(english.toStdString());
    return it == table.end() ? english : juce::String::fromUTF8(it->second.c_str());
}

std::size_t translationCount(Language lang) {
    switch (lang) {
        case Language::Spanish: return spanishTable().size();
        case Language::Portuguese: return portugueseTable().size();
        case Language::Italian: return italianTable().size();
        case Language::Chinese: return chineseTable().size();
        case Language::French: return frenchTable().size();
        case Language::German: return germanTable().size();
        case Language::Japanese: return japaneseTable().size();
        case Language::Russian: return russianTable().size();
        case Language::Korean: return koreanTable().size();
        case Language::TraditionalChinese: return traditionalChineseTable().size();
        case Language::Turkish: return turkishTable().size();
        case Language::Polish: return polishTable().size();
        case Language::Dutch: return dutchTable().size();
        case Language::Swedish: return swedishTable().size();
        case Language::Indonesian: return indonesianTable().size();
        case Language::Vietnamese: return vietnameseTable().size();
        case Language::Ukrainian: return ukrainianTable().size();
        case Language::Finnish: return finnishTable().size();
        case Language::Catalan: return catalanTable().size();
        case Language::Hindi: return hindiTable().size();
        default: return 0;
    }
}


}  // namespace i18n
