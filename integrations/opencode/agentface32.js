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

function debugLog(message) {
  if (!process.env.AGENTFACE_DEBUG) return
  try {
    // Dynamic import keeps the normal path free of Node-specific modules.
    import("node:fs").then((fs) => {
      const path = (process.env.TEMP || "/tmp") + "/agentface-opencode.log"
      fs.appendFileSync(path, new Date().toISOString() + " " + message + "\n")
    })
  } catch {
    // Debug logging must never break the agent.
  }
}

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
    if (!event) return
    // V2 puts the payload in event.data; V1 used event.properties.
    const data = event.data || event.properties || {}
    const info = data.info || {}
    const sessionID = toolName(data.sessionID || info.id)
    const parentID = toolName(data.parentID || info.parentID)
    const isChild = Boolean(parentID) || childSessions.has(sessionID)
    debugLog("event " + JSON.stringify(event).slice(0, 1500))

    switch (event.type) {
      case "session.created": {
        if (parentID && sessionID) childSessions.add(sessionID)
        send({ hook_event_name: parentID ? "SubagentStart" : "SessionStart", source: "startup" })
        break
      }
      case "session.deleted": {
        if (sessionID) childSessions.delete(sessionID)
        send({ hook_event_name: isChild ? "SubagentStop" : "SessionEnd" })
        break
      }
      case "session.execution.started":
        // Child sessions start while the main task is already running, so
        // only a root session starts a new task on the display.
        if (!isChild) send({ hook_event_name: "UserPromptSubmit" })
        break
      case "session.execution.succeeded":
        if (!isChild) send({ hook_event_name: "Stop" })
        break
      case "session.execution.failed":
        if (!isChild) send({ hook_event_name: "StopFailure", error: shortError(data.error) })
        break
      case "session.execution.interrupted":
        // "superseded" means a newer prompt replaced this execution; the next
        // execution.started follows immediately, so do not flash "cancelled".
        if (!isChild && data.reason !== "superseded") {
          send({ hook_event_name: "Interrupt", reason: toolName(data.reason) })
        }
        break
      case "session.compaction.started":
        send({ hook_event_name: "PreCompact" })
        break
      case "session.compaction.ended":
        send({ hook_event_name: "PostCompact" })
        break
      // V1 event names, kept for older opencode builds.
      case "session.idle":
        send({ hook_event_name: isChild ? "SubagentStop" : "Stop" })
        break
      case "session.error":
        send({ hook_event_name: isChild ? "SubagentStop" : "StopFailure", error: shortError(data.error) })
        break
      case "session.compacted":
        send({ hook_event_name: "PostCompact" })
        break
      case "permission.asked":
        send({
          hook_event_name: "PermissionRequest",
          tool_name: toolName(data.action || data.permission || data.tool || data.title),
        })
        break
      case "permission.replied": {
        // opencode uses "once" / "always" / "reject".
        const reply = String(data.response || data.reply || "").toLowerCase()
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
        debugLog("event subscription starting")
        for await (const event of ctx.event.subscribe({ signal: controller.signal })) {
          debugLog("event received " + (event && event.type))
          tracker.handleEvent(event)
        }
        debugLog("event subscription ended")
      } catch (error) {
        debugLog("event subscription failed: " + (error && error.message))
      }
    })()

    try {
      // The prompt hook gives an early task start; session.execution.started
      // repeats it if the event subscription was established late. End comes
      // from session.execution.* events.
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
