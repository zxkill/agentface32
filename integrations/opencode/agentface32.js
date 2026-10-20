// AgentFace32 plugin for opencode.
//
// Copy this file into one of the plugin directories and restart opencode:
//   .opencode/plugins/agentface32.js            (per project)
//   ~/.config/opencode/plugins/agentface32.js   (global)
//
// The same file supports opencode V1 (1.18.29+) and V2:
//   - V2 calls the default export's setup(ctx) and its tool/session/event hooks;
//   - V1 calls server() and its returned hook object.
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

// Both APIs funnel into this small state machine. It also tracks child
// sessions so a subagent finishing does not mark the whole task as done.
function createTracker() {
  const childSessions = new Set()

  function handleEvent(event) {
    const props = (event && event.properties) || {}
    const info = props.info || {}
    const isChild = Boolean(info.parentID) || childSessions.has(props.sessionID)

    switch (event && event.type) {
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
  }

  return {
    handleEvent,
    handlePrompt(event) {
      if (event && event.sessionID && childSessions.has(event.sessionID)) return
      send({ hook_event_name: "UserPromptSubmit" })
    },
    handleToolBefore(event) {
      send({
        hook_event_name: "PreToolUse",
        tool_name: toolName(event && event.tool),
        tool_input: (event && event.input) || {},
      })
    },
    handleToolAfter(event) {
      if (event && event.status === "error") {
        send({
          hook_event_name: "PostToolUseFailure",
          tool_name: toolName(event.tool),
          error: shortError(event.error && event.error.message),
        })
        return
      }
      send({
        hook_event_name: "PostToolUse",
        tool_name: toolName(event && event.tool),
      })
    },
  }
}

const agentface32 = {
  id: "agentface32",

  // opencode V2
  async setup(ctx) {
    const tracker = createTracker()

    const controller = new AbortController()
    void (async () => {
      try {
        for await (const event of ctx.event.subscribe({ signal: controller.signal })) {
          tracker.handleEvent(event)
        }
      } catch {
        // The server is shutting down or the subscription was aborted.
      }
    })()

    try {
      await ctx.session.hook("prompt", (event) => tracker.handlePrompt(event))
      await ctx.tool.hook("execute.before", (event) => tracker.handleToolBefore(event))
      await ctx.tool.hook("execute.after", (event) => tracker.handleToolAfter(event))
    } catch {
      // Older V2 builds may expose fewer domains; the event stream still works.
    }

    return () => controller.abort()
  },

  // opencode V1 (1.18.29+)
  async server() {
    const tracker = createTracker()

    return {
      event: async ({ event }) => tracker.handleEvent(event),
      "chat.message": async (input) => tracker.handlePrompt(input),
      "tool.execute.before": async (input, output) => {
        tracker.handleToolBefore({ tool: input.tool, input: output.args })
      },
      "tool.execute.after": async (input) => {
        tracker.handleToolAfter({ tool: input.tool })
      },
    }
  },
}

export default agentface32
