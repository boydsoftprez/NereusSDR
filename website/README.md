# NereusSDR website

The public site at https://nereussdr.com/, served by Caddy from a DigitalOcean
droplet.

- `public/` is the deployable site: static HTML, CSS, JS and images.
- The user guide and how-to pages are generated from the repository Markdown
  with `docs/manual/build_preview.py`; edit the manuscripts, then regenerate.
- `deploy/` is the server side: `Caddyfile` (the web server config) and
  `setup-server.sh` (one-time, re-runnable server setup).
- `deploy.sh` publishes `public/` to the server with rsync.

## Preview locally

```sh
python3 -m http.server 8765 --directory website/public
```

Then open http://localhost:8765/. The user guide is at `/manual/`, and the
station how-to pages are under `/guides/`. These routes are generated site
files, not redirects to GitHub. The preview does not send the headers Caddy
adds in production (including the Content-Security-Policy), so check the live
site after a deploy as well.

## Build the user guide and how-to pages

Python 3 and `markdown-it-py` are required, as for the existing manual preview.
Pillow lets the source checker verify the preserved raster captures.

```sh
python3 docs/manual/build_preview.py --website
python3 docs/manual/check_manual.py
python3 docs/manual/test_build_preview.py
```

The website build renders the manual index and all 20 chapters to
`public/manual/`, and renders the SBC installation, TX EQ/CFC, upgrade and
remote-access guides to `public/guides/`. It uses the same Markdown chapters,
images and manual layout as the local manual preview. Its `manual/source-map.json`
records each rendered document's source path and hash, and the copied image
hashes. Generated pages should be committed with their source updates; do not
edit generated prose directly. Regenerate after a manuscript or manual renderer
change and before deployment.

Relative operator links point to generated pages on this site. Links to
repository source, contributor records, release assets and technical references
keep their appropriate source/download destinations. The build preserves the
manual's build, capture and mobile-availability qualifications.

## One-time server setup

1. On the Mac, add the deploy alias to `~/.ssh/config`:

   ```
   Host nereus-web
       HostName <droplet IPv4>
       User nereusweb
       IdentityFile ~/.ssh/nereus_vps_ed25519
       IdentitiesOnly yes
   ```

   The scripts reach the server only through this alias, so the address never
   appears in the repository.

2. As root on the droplet (`root@<droplet>` is whatever root login already
   works), from the repository root on the Mac:

   ```sh
   scp website/deploy/Caddyfile website/deploy/setup-server.sh root@<droplet>:/root/
   ssh root@<droplet> "bash /root/setup-server.sh /root/Caddyfile '$(cat ~/.ssh/nereus_vps_ed25519.pub)'"
   ```

   The script installs Caddy from the official Caddy apt repository, creates the
   `nereusweb` deploy account with that public key, prepares
   `/var/www/nereussdr`, validates and installs the Caddyfile, and starts Caddy.
   It is safe to run again, for example after editing the Caddyfile. It does
   not touch the firewall, the SSH daemon or any other service on the droplet.

## DNS at Namecheap

| Type | Host | Value |
| ---- | ---- | ----- |
| A    | `@`   | `<droplet IPv4>` |
| AAAA | `@`   | `<droplet IPv6>` |
| A    | `www` | `<droplet IPv4>` |
| AAAA | `www` | `<droplet IPv6>` |

If Namecheap's default parking records for `@` and `www` are still there,
delete them first. Caddy obtains the HTTPS certificates by itself once both
names resolve to the droplet, and keeps retrying until they do.

## Deploy

```sh
website/deploy.sh --dry-run   # list what would change
website/deploy.sh             # publish
```

rsync runs with `--delete`, so the server ends up with exactly what is in
`public/`, minus `.DS_Store`, `*.swp` and `.git*` files. The script refuses to
run when `public/index.html` is missing or `public/` holds fewer than 3 files.
Set `NEREUS_WEB_TARGET` to a local directory to try it without the server.

## Why HTTP/3 is off

Caddy normally serves HTTP/3 as well, and HTTP/3 runs over QUIC on UDP port
443. On this droplet UDP 443 is already used by another service, so the
Caddyfile limits Caddy to HTTP/1.1 and HTTP/2 (`protocols h1 h2`), which keeps
it on TCP 80 and TCP 443 only. Browsers fall back to HTTP/2 without any visible
difference. `setup-server.sh` fails if Caddy ever holds a UDP port, so HTTP/3
cannot come back unnoticed while that other service shares the host.
