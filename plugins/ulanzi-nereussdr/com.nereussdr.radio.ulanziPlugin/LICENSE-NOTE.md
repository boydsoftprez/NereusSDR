# License note

This plugin is part of NereusSDR and is licensed under the GNU General Public License,
version 3 (see `LICENSE` at the root of the NereusSDR repository). Written for NereusSDR
by J.J. Boyd (KG4VCF), with AI assistance from Anthropic Claude Code, after studying how
AetherSDR's plugin (GPL-3, Jeremy (KK7GWY) / AetherSDR contributors) talks to the same host.
No AetherSDR code is copied. It calls no TCI: it talks only to the NereusSDR window, over a
private local connection.

Third-party files here keep their own licenses: `libs/` is the Ulanzi Studio plugin SDK, Apache-2.0, with its `LICENSE.txt` files beside it, and `node_modules/ws` is the `ws` library, MIT, with its `LICENSE` file beside it. Two SDK files were left out of `libs/`: `common-node/test.js` and each `.gitignore`.
