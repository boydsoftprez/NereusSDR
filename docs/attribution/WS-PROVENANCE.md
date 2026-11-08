# ws Provenance - NereusSDR vendored-library inventory

`ws` is the WebSocket client the Elgato and Ulanzi plugins use to reach their host apps. It
is vendored unchanged, twice:
`plugins/elgato-nereussdr/com.nereussdr.radio.sdPlugin/node_modules/ws` and
`plugins/ulanzi-nereussdr/com.nereussdr.radio.ulanziPlugin/node_modules/ws`. It is not linked
into any binary.

## Upstream

- **Project:** ws (https://github.com/websockets/ws)
- **Version:** 8.21.0, as its `package.json` says
- **Taken from:** AetherSDR `plugins/elgato-aethersdr/com.aethersdr.radio.sdPlugin/node_modules/ws`
  at `1e0718ad`, not from the npm registry
- **Vendored:** 2026-11-06 (hot keys plan, Task 20)
- **SHA-256 of the sorted per-file SHA-256 list:** `395325dfdd46537a2017da63b6cc89b5aab4bee7aac6bc0d1cc6ce4d09fc2287`

## License

MIT. The text is `LICENSE` beside the code, and `packaging/third-party-licenses/ws.txt`.
