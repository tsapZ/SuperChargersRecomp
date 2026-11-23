#include "localization.h"
#include "portal/system_locale.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Defined in the SDK (xam_user.cpp); both XGetLanguage and ExGetXConfigSetting read this.
REXCVAR_DECLARE(uint32_t, user_language);
// Defined in the SDK (xam_locale.cpp): the console's country (XC_COUNTRY ids), which XGetLocale reads.
REXCVAR_DECLARE(uint32_t, user_country);

REXCVAR_DEFINE_STRING(language, "auto", "Game",
                      "Game text/voice language: auto (the Windows display language if it is one "
                      "of the following, else English), english, french, german, spanish, "
                      "italian, dutch, swedish, norwegian, danish, finnish (each disc has two or "
                      "four of them; one it lacks gives the disc's first). A raw Xbox 360 language "
                      "ID (a number) also works, for experiments.");

namespace superchargersrecomp {

namespace {

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// The Windows display language as one of the names below; English for any other.
std::string SystemLanguage() {
#ifdef _WIN32
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_GERMAN:
      return "german";
    case LANG_FRENCH:
      return "french";
    case LANG_SPANISH:
      return "spanish";
    case LANG_ITALIAN:
      return "italian";
    case LANG_DUTCH:
      return "dutch";
    case LANG_SWEDISH:
      return "swedish";
    case LANG_NORWEGIAN:
      return "norwegian";
    case LANG_DANISH:
      return "danish";
    case LANG_FINNISH:
      return "finnish";
    default:
      break;
  }
  return "english";
#else
  const std::string lang = skylanders::PosixSystemLanguage();
  if (lang == "german" || lang == "french" || lang == "spanish" || lang == "italian" ||
      lang == "dutch" || lang == "swedish" || lang == "norwegian" || lang == "danish" ||
      lang == "finnish") {
    return lang;
  }
  return "english";
#endif
}

}  // namespace

void ApplyLanguageSetting() {
  // A direct --user_language / user_language=<n> override (raw, for experimentation) always
  // wins: don't clobber it with whatever `language` defaults to (a real bug this had until now --
  // it unconditionally overwrote user_language, silently discarding a direct override).
  if (rex::cvar::HasNonDefaultValue("user_language")) {
    REXLOG_INFO("Language: user_language already set directly ({}); leaving it alone",
               REXCVAR_GET(user_language));
    return;
  }

  // Xbox 360 XLanguage IDs (include/rex/system/xcontent.h). The game turns them into its own ids in
  // sub_824D7E10 (English 1 -> 6, Swedish 13 -> 23, Norwegian 15 -> 17, Dutch 16 -> 5, ...) and, in
  // sub_828D2908, uses the disc's first language when the one asked for isn't on the disc (English on
  // the Es/It disc starts in its own language, 2026-11-22).
  static const std::unordered_map<std::string, uint32_t> kNames = {
      {"english", 1}, {"german", 3},    {"french", 4},     {"spanish", 5},
      {"italian", 6}, {"swedish", 13}, {"norwegian", 15},
  };
  // The country to go with a language, when not set directly (Core of Light #118): Danish and Finnish have no
  // Xbox 360 language; sub_828D2908 takes them from the locale DK / FI (the SDK's xam_locale.cpp: country
  // 25 -> DK, 32 -> FI), with English as the language. Swedish and Norwegian get their country as a console
  // there would.
  static const std::unordered_map<std::string, uint32_t> kCountries = {
      {"danish", 25u}, {"finnish", 32u}, {"swedish", 90u}, {"norwegian", 75u},
  };

  std::string setting = Lower(REXCVAR_GET(language));
  if (setting == "auto") {
    setting = SystemLanguage();
    REXLOG_INFO("Language: auto, from the Windows display language");
  }
  // Vicarious Visions reads Xbox 360 XLanguage::kDutch (16) via ExGetXConfigSetting(3, 9) in
  // sub_83567248 -> sub_824D7E10 (VV language 5), unlike Toys for Bob's Spyro's Adventure which
  // took Dutch from user_country alone. Also set user_country to XC_COUNTRY_NL (74) when unset so
  // XamGetLocaleEx reports NL. A country set directly (--user_country) stays.
  if (setting == "dutch") {
    REXCVAR_SET(user_language, 16u);
    if (!rex::cvar::HasNonDefaultValue("user_country")) {
      REXCVAR_SET(user_country, 74u);  // XC_COUNTRY_NL
    }
    REXLOG_INFO("Language: dutch (language 16, country {})", REXCVAR_GET(user_country));
    return;
  }
  if (auto c = kCountries.find(setting); c != kCountries.end() &&
                                         !rex::cvar::HasNonDefaultValue("user_country")) {
    REXCVAR_SET(user_country, c->second);
  }
  if (setting == "danish" || setting == "finnish") {
    REXCVAR_SET(user_language, 1u);
    REXLOG_INFO("Language: {} (English with the country {})", setting, REXCVAR_GET(user_country));
    return;
  }
  if (auto it = kNames.find(setting); it != kNames.end()) {
    REXCVAR_SET(user_language, it->second);
    REXLOG_INFO("Language: {} (id {}, country {})", setting, it->second, REXCVAR_GET(user_country));
    return;
  }

  uint32_t numeric_id = 0;
  auto [ptr, ec] = std::from_chars(setting.data(), setting.data() + setting.size(), numeric_id);
  if (ec == std::errc() && ptr == setting.data() + setting.size()) {
    REXCVAR_SET(user_language, numeric_id);
    REXLOG_INFO("Language: raw id {}", numeric_id);
    return;
  }

  REXLOG_WARN("Language: '{}' is not a known name or a number; using English", setting);
  REXCVAR_SET(user_language, 1u);
}

}  // namespace superchargersrecomp
