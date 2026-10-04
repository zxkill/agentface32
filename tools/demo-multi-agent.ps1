param([Parameter(Mandatory=$true)][string]$Ip)
$base = "http://$Ip"
function Hook($agent, $json) {
  Invoke-RestMethod -Method Post -Uri "$base/hook/$agent" -ContentType 'application/json' -Body ($json | ConvertTo-Json -Depth 8 -Compress) | Out-Null
}
Invoke-RestMethod -Method Post "$base/view?mode=both" | Out-Null
Hook claude @{hook_event_name='UserPromptSubmit'}
Start-Sleep 1
Hook claude @{hook_event_name='PreToolUse';tool_name='Read';tool_input=@{file_path='src/auth/service.cpp'}}
Start-Sleep 1
Hook codex @{hook_event_name='UserPromptSubmit'}
Hook codex @{hook_event_name='PreToolUse';tool_name='Bash';tool_input=@{command='pio test'}}
Start-Sleep 2
Hook claude @{hook_event_name='PreToolUse';tool_name='Edit';tool_input=@{file_path='src/auth/service.cpp'}}
Start-Sleep 2
Hook codex @{hook_event_name='Stop'}
Start-Sleep 2
Hook claude @{hook_event_name='Stop'}
Write-Host 'Demo complete.'
