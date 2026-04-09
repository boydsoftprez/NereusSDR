# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in NereusSDR, please report it
**privately** rather than opening a public issue.

**Email:** Send details to the repository owner via GitHub private messaging,
or use [GitHub's private vulnerability reporting](https://github.com/boydsoftprez/NereusSDR/security/advisories/new).

Please include:
- A description of the vulnerability
- Steps to reproduce
- Potential impact

We will acknowledge your report within 72 hours and work with you on a fix
before any public disclosure.

## Scope

NereusSDR communicates with OpenHPSDR-compatible radios over local network
protocols (UDP for Protocol 1, UDP multi-port for Protocol 2). Security concerns
include:

- Buffer overflows in OpenHPSDR protocol packet parsing (1032-byte P1 Metis frames, P2 UDP data)
- Command injection via malformed protocol responses
- Denial of service from crafted UDP packets on port 1024
- Sensitive data exposure (e.g. network addresses in logs)
- WDSP buffer handling (malformed I/Q data causing out-of-bounds access)
- WebSocket server endpoints (TCI, future integrations)

## Supported Versions

Only the latest version on the `main` branch receives security fixes.
