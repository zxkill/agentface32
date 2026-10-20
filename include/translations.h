#pragma once
#include <Arduino.h>
#include "state.h"

bool localeIsRussian();
const char* tr(const char* en, const char* ru);
String trString(const char* en, const char* ru);
const char* stateTitle(AgentState state);
