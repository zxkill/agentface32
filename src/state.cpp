#include "state.h"

const char* stateName(AgentState state) {
  switch (state) {
    case AgentState::Idle: return "IDLE";
    case AgentState::Welcome: return "HELLO";
    case AgentState::Reading: return "READING";
    case AgentState::Thinking: return "THINKING";
    case AgentState::Typing: return "EDITING";
    case AgentState::Running: return "RUNNING";
    case AgentState::Attention: return "NEED_INPUT";
    case AgentState::Done: return "DONE";
    case AgentState::Error: return "ERROR";
    case AgentState::Abort: return "CANCELLED";
    case AgentState::Sleep: return "SLEEP";
  }
  return "UNKNOWN";
}


const char* stateTitleEn(AgentState state) {
  switch (state) {
    case AgentState::Idle: return "IDLE";
    case AgentState::Welcome: return "ONLINE";
    case AgentState::Reading: return "READING";
    case AgentState::Thinking: return "THINKING";
    case AgentState::Typing: return "EDITING";
    case AgentState::Running: return "RUNNING";
    case AgentState::Attention: return "NEEDS YOU";
    case AgentState::Done: return "DONE";
    case AgentState::Error: return "ERROR";
    case AgentState::Abort: return "CANCELLED";
    case AgentState::Sleep: return "SLEEP";
  }
  return "UNKNOWN";
}

const char* stateTitleRu(AgentState state) {
  switch (state) {
    case AgentState::Idle: return "ЖДУ";
    case AgentState::Welcome: return "НА СВЯЗИ";
    case AgentState::Reading: return "ЧИТАЮ";
    case AgentState::Thinking: return "ДУМАЮ";
    case AgentState::Typing: return "ПИШУ";
    case AgentState::Running: return "ВЫПОЛНЯЮ";
    case AgentState::Attention: return "НУЖНО ВНИМАНИЕ";
    case AgentState::Done: return "ГОТОВО";
    case AgentState::Error: return "ОШИБКА";
    case AgentState::Abort: return "ОТМЕНЕНО";
    case AgentState::Sleep: return "СПЛЮ";
  }
  return "НЕИЗВЕСТНО";
}

bool isOneShotState(AgentState state) {
  return state == AgentState::Welcome || state == AgentState::Attention ||
         state == AgentState::Done || state == AgentState::Error ||
         state == AgentState::Abort;
}
