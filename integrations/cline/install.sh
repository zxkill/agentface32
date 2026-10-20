#!/usr/bin/env bash
# Install AgentFace32 Cline hooks into the current workspace (Unix only:
# Cline itself does not run hooks on Windows yet).
#
#   ./integrations/cline/install.sh                  # .clinerules/hooks
#   ./integrations/cline/install.sh ~/Documents/Cline/Hooks
#
# Set AGENTFACE_URL before launching Cline if the device is not reachable as
# http://agentface32.local (for example: export AGENTFACE_URL=http://192.168.1.50).
set -euo pipefail

src="$(cd "$(dirname "$0")" && pwd)"
dest="${1:-.clinerules/hooks}"

mkdir -p "$dest"
cp "$src/agentface32-hook.py" "$dest/agentface32-hook.py"
chmod +x "$dest/agentface32-hook.py"

for hook in TaskStart TaskResume TaskCancel TaskComplete UserPromptSubmit PreToolUse PostToolUse PreCompact; do
  cat > "$dest/$hook" <<'EOF'
#!/usr/bin/env bash
exec python3 "$(dirname "$0")/agentface32-hook.py"
EOF
  chmod +x "$dest/$hook"
done

echo "Installed Cline hooks into $dest"
echo "Enable them in Cline settings, then restart the extension."
