# Security

## Supported version

Security fixes target the latest public release and `main`.

## Important network model

AgentFace32 is designed for a **trusted local network**. Its HTTP API is currently unauthenticated. Do not port-forward the ESP32, place it directly on the public internet, or use it on an untrusted Wi-Fi network without additional network isolation.

Lifecycle payloads can contain development metadata such as:

- local file paths
- shell commands
- search terms
- URLs
- tool names

AgentFace32 does not need Anthropic/OpenAI API keys and does not intentionally send hook data to a cloud service.

## Reporting a vulnerability

Prefer GitHub private vulnerability reporting if it is enabled for the repository. Otherwise contact the repository owner privately before opening a public issue for a vulnerability that could expose users.

Please include the affected version/profile, reproduction steps and realistic impact.
