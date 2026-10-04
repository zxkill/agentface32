param([Parameter(Mandatory=$true)][string]$Ip)
$base = "http://$Ip"
Write-Host "Health:" -ForegroundColor Cyan
Invoke-RestMethod "$base/health" | ConvertTo-Json -Depth 5
Write-Host "Claude -> reading" -ForegroundColor Cyan
Invoke-RestMethod -Method Post "$base/anim?agent=claude&name=reading" | Out-Null
Start-Sleep -Seconds 1
Write-Host "Codex -> running" -ForegroundColor Cyan
Invoke-RestMethod -Method Post "$base/anim?agent=codex&name=running" | Out-Null
Start-Sleep -Seconds 1
Write-Host "Both view" -ForegroundColor Cyan
Invoke-RestMethod -Method Post "$base/view?mode=both" | Out-Null
Invoke-RestMethod "$base/state" | ConvertTo-Json -Depth 5
