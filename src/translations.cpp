#include "translations.h"
#include "settings.h"

bool localeIsRussian() { return settings().russian; }
const char* tr(const char* en, const char* ru) { return localeIsRussian() ? ru : en; }
String trString(const char* en, const char* ru) { return String(tr(en, ru)); }

const char* stateTitle(AgentState state) {
  if (localeIsRussian()) return stateTitleRu(state);
  return stateTitleEn(state);
}
