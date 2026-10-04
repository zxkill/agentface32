#include <string.h>
#include "locale.h"
#include "config.h"

bool localeIsRussian() { return strcmp(UI_LANGUAGE, "ru") == 0; }
const char* tr(const char* en, const char* ru) { return localeIsRussian() ? ru : en; }
String trString(const char* en, const char* ru) { return String(tr(en, ru)); }

const char* stateTitle(AgentState state) {
  if (localeIsRussian()) return stateTitleRu(state);
  return stateTitleEn(state);
}
