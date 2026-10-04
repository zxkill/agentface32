#!/usr/bin/env bash
set -euo pipefail
IP="${1:?usage: $0 DEVICE_IP}"
BASE="http://$IP"
post() { curl -fsS -o /dev/null -X POST -H 'Content-Type: application/json' --data "$3" "$BASE/hook/$2"; }
curl -fsS -o /dev/null -X POST "$BASE/view?mode=both"
post x claude '{"hook_event_name":"UserPromptSubmit"}'
sleep 1
post x claude '{"hook_event_name":"PreToolUse","tool_name":"Read","tool_input":{"file_path":"src/auth/service.cpp"}}'
sleep 1
post x codex '{"hook_event_name":"UserPromptSubmit"}'
post x codex '{"hook_event_name":"PreToolUse","tool_name":"Bash","tool_input":{"command":"pio test"}}'
sleep 2
post x claude '{"hook_event_name":"PreToolUse","tool_name":"Edit","tool_input":{"file_path":"src/auth/service.cpp"}}'
sleep 2
post x codex '{"hook_event_name":"Stop"}'
sleep 2
post x claude '{"hook_event_name":"Stop"}'
echo 'Demo complete.'
