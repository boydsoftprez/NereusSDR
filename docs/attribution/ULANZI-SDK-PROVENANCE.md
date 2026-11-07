# Ulanzi Studio plugin SDK Provenance - NereusSDR vendored-library inventory

Ulanzi's SDK connects the Ulanzi plugin to Ulanzi Studio. It lives in
`plugins/ulanzi-nereussdr/com.nereussdr.radio.ulanziPlugin/libs/` (`common-node` for the
plugin, `common-html` for its settings page). It is not linked into any binary.

## Upstream

- **Project:** UlanziDeckPlugin-SDK (https://github.com/UlanziTechnology/UlanziDeckPlugin-SDK,
  as its README links it)
- **Version:** `common-node` says 0.1.0 in its `package.json`. The upstream commit isn't known.
- **Taken from:** AetherSDR `plugins/ulanzi-aethersdr/com.g0jkn.aethersdr.ulanziPlugin/libs` at
  `1e0718ad`
- **Vendored:** 2026-11-06 (hot keys plan, Task 20)
- **SHA-256 of the sorted per-file SHA-256 list, AetherSDR's copy:** `59185a677eea2da95740d9ef2cca8e71741d57b5df7134674f3beb68a00c3378`

## Changes

No file is changed. Left out: `common-node/test.js`, `common-node/.gitignore`,
`common-html/.gitignore`.

## License

Apache-2.0. The text is `LICENSE.txt` in each folder, `packaging/third-party-licenses/ulanzi-sdk.txt`
and `packaging/third-party-licenses/Apache-2.0.txt`.
