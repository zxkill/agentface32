#pragma once
#include <Arduino.h>
#include "state.h"

void audioBegin();
void audioLoop();
void audioSetEnabled(bool enabled);
bool audioIsEnabled();
void audioToggle();
void audioButtonClick();
void audioForState(AgentState state);
