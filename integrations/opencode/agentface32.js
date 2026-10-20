// AgentFace32 plugin for opencode.
//
// Copy this file into one of the plugin directories and restart opencode:
//   .opencode/plugins/agentface32.js            (per project)
//   ~/.config/opencode/plugins/agentface32.js   (global)
//
// The device is expected at http://agentface32.local. Override it with the
// AGENTFACE_URL environment variable, e.g. AGENTFACE_URL=http://192.168.1.50.
//
// Events are sent fire-and-forget: opencode never waits for the display and
// keeps working when the device is offline.

const DEFAULT_URL = "http://agentface32.local"
const TIMEOUT_MS = 1500

function send(payload) {
  const base = (process.env.AGENTFACE_URL || DEFAULT_URL).replace(/\/+$/, "")
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS)

  fetch(base + "/hook/opencode", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
    signal: controller.signal,
  })
    .catch(() => {})
    .finally(() => clearTimeout(timer))
}

function toolName(value) {
  return typeof value === "string" ? value : ""
}

function shortError(value) {
  if (value == null) return ""
  const text = typeof value === "string" ? value : JSON.stringify(value)
  return text.length > 200 ? text.slice(0, 200) : text
}

export const AgentFace32 = async () => {
  // opencode runs subagents in child sessions. Remember them so their idle
  // events do not mark the whole task as finished.
  const childSessions = new Set()

  return {
  event: async ({ event }) => {
    const props = event.properties || {}
    const info = props.info || {}
    const isChild = Boolean(info.parentID) || childSessions.has(props.sessionID)

    switch (event.type) {
      case "session.created": {
        const child = Boolean(info.parentID)
        if (child && info.id) childSessions.add(info.id)
        send({ hook_event_name: child ? "SubagentStart" : "SessionStart", source: "startup" })
        break
      }
      case "session.deleted": {
        if (info.id) childSessions.delete(info.id)
        send({ hook_event_name: isChild ? "SubagentStop" : "SessionEnd" })
        break
      }
      case "session.idle":
        send({ hook_event_name: isChild ? "SubagentStop" : "Stop" })
        break
      case "session.error":
        send({ hook_event_name: isChild ? "SubagentStop" : "StopFailure", error: shortError(props.error) })
        break
      case "session.compacted":
        send({ hook_event_name: "PostCompact" })
        break
      case "permission.asked":
        send({
          hook_event_name: "PermissionRequest",
          tool_name: toolName(props.permission || props.tool || props.title),
        })
        break
      case "permission.replied": {
        // opencode uses e.g. "once" / "always" / "reject".
        const reply = String(props.response || props.reply || "").toLowerCase()
        const rejected = reply === "reject" || reply === "rejected" || reply === "deny" || reply === "denied"
        send({ hook_event_name: rejected ? "PermissionDenied" : "PermissionGranted" })
        break
      }
      default:
        break
    }
  },

  "chat.message": async (input) => {
    // Child-session messages are agent-internal, not user prompts.
    if (input && input.sessionID && childSessions.has(input.sessionID)) return
    send({ hook_event_name: "UserPromptSubmit" })
  },

  "tool.execute.before": async (input, output) => {
    send({
      hook_event_name: "PreToolUse",
      tool_name: toolName(input.tool),
      tool_input: output.args || {},
    })
  },

  "tool.execute.after": async (input) => {
    send({
      hook_event_name: "PostToolUse",
      tool_name: toolName(input.tool),
      tool_input: input.args || {},
    })
  },
  }
}
