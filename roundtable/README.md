# NereusSDR Roundtable

Roundtable groups stations into rooms by frequency, shows each station's
own S-meter reading of whoever is talking, works out turns, and carries
short messages about each frequency. Stations reach it over a WebSocket;
anyone can read the live rooms. Its wire is
[the Roundtable document](../docs/architecture/2026-10-23-roundtable-v1.md),
and that document is the authority for every protocol detail.

NereusSDR uses `roundtable.nereussdr.com`. Anyone can run their own server;
every default is nereussdr.com's, and each one is a setting.

- `server/`: the server, a small Python program (`nereus_roundtable`).
  Caddy sits in front of it for TLS and forwards by path to its two
  loopback listeners: WebSockets on port 8720 (`/v1/station`, `/v1/live`)
  and HTTP on port 8721 (the read API and the web pages).
  `roundtable.conf.sample` lists every setting with its default.
- `conformance/v1/`: the fixtures the server, the Core and other station
  software all run: crypto vectors, one message per control fixture,
  scripted sessions and scripted rooms (the document's section 11).
- `tests/`: the server's tests, its conformance runner among them.

The server keeps no client address in any file or log. Addresses are held
in memory only, to count connections per network, and a log line names at
most the first 6 characters of a station id.

## Running the tests

The server needs Python 3.9 or later with `websockets` (10.4 or later) and
`cryptography`, and the tests need `pytest`. From the repository root:

```
python3 -m pytest roundtable/tests -q -p no:cacheprovider
```

The authoritative run is Ubuntu 24.04's Python 3.12 with apt's
`python3-websockets`, `python3-cryptography` and `python3-pytest`, which is
what CI's `roundtable` job installs. The tests also run on macOS's
`/usr/bin/python3` (3.9) with websockets 15. `server/nereus_roundtable/transport.py`
holds every difference between the two websockets APIs.

The tests need no network beyond loopback and write nothing outside
pytest's temporary directories. The conformance runner plays every
connection of a session fixture against a real server on loopback, with a
clock that moves only when a fixture says so.

## Running the server

```
cd roundtable/server
python3 -m nereus_roundtable --config /etc/nereus-roundtable/roundtable.conf
```

An unknown section or key, or a value the server cannot use, stops it at
start with exit code 2 and a one-line message.

## The load probe

`tests/load_probe.py` runs the real server in its own process and loads it
over loopback WebSockets from client processes: stations that send a
presence every 10 s and a reading once a second, take turns keying, post
(half the posts naming another station), move frequency now and then and
sign in again every 15 minutes on average, and live viewers that watch a
band and up to 4 of its rooms. It prints the server's peak memory and CPU
per wall second, the band-list tick, the event loop's lateness, the cost of
each sign-in's mention lookup, and the earlier turns the server holds:

```
python3 roundtable/tests/load_probe.py --stations 500 --rooms 150 --viewers 500 --minutes 30
```

It exits 1 when the peak memory reaches 256 MB or the CPU reaches half of
one core. `--mentions-bench N` times the mention lookup alone against a
table of N messages. It is not part of the pytest run.

## Deploying

The server runs on the website's server (`nereus-web`), behind the
website's Caddy. Everything is in `deploy/`:

- `nereus-roundtable.service`: the server's unit. A transient account
  (`DynamicUser`), the database in `/var/lib/nereus-roundtable`, loopback
  only with no outbound network, `MemoryMax=256M` with no swap, and the
  rendezvous unit's sandbox. Under memory pressure the kernel ends it before
  Caddy (`OOMScoreAdjust=500`) and systemd starts it again.
- `nereus-roundtable-daily.service` and `.timer`: the daily job at 04:00
  UTC (text older than 365 days cleared, a journal line with the database's
  size and the free disk, a warning under 20% free).
- `nereus-roundtable-snapshot.service` and `.timer`: the nightly snapshot at
  03:30 UTC (below).
- `roundtable.caddy`: the site block, installed as
  `/etc/caddy/sites/roundtable.caddy`. `/v1/station` and `/v1/live` go to
  port 8720, everything else to port 8721, each with the client's own
  address as the one `X-Forwarded-For`. No access log. The website's
  Caddyfile pulls it in with `import /etc/caddy/sites/*.caddy`, and the
  website's `setup-server.sh` makes that folder, so installing the
  website's Caddyfile again keeps this site.
- `caddy-override.conf`: Caddy's drop-in (`Restart=on-failure`,
  `OOMScoreAdjust=-100`, a `GOMEMLIMIT` from the server's memory), the one
  the rendezvous server has.
- `setup-server.sh`: installs all of it. Run as root on the server.
- `pull-backup.sh`: run on JJ's machine; pulls the newest snapshot.

`deploy.sh` (beside this README) publishes the code.

1. DNS: `roundtable` A and AAAA records pointing at the website's server.
   Caddy obtains the certificate once they resolve.
2. The website first: its current `website/deploy/Caddyfile` must be
   installed (with the import line), by `website/deploy/setup-server.sh`
   as `website/README.md` says.
3. From the repository root on the Mac (the rm keeps a second copy from
   landing inside the first):

   ```sh
   ssh root@<droplet> rm -rf /root/roundtable
   scp -r roundtable root@<droplet>:/root/roundtable
   ssh root@<droplet> bash /root/roundtable/deploy/setup-server.sh --dry-run
   ssh root@<droplet> bash /root/roundtable/deploy/setup-server.sh
   ```

   The dry run prints every step and what it would change, and changes
   nothing. The real run installs Python's packages from Ubuntu, makes
   `/opt/nereus-roundtable` for the website's deploy account (`nereusweb`),
   writes `/etc/nereus-roundtable/roundtable.conf` from the sample when
   there is none (an existing one is never changed), installs the units,
   the timers and Caddy's drop-in, validates Caddy with the new site file
   before installing it, then enables and starts what it installed and
   reloads Caddy. `RT_HOST` changes the host name; the script's header lists
   every setting. It is safe to run again.
4. Publish the code, then start the server:

   ```sh
   roundtable/deploy.sh --dry-run
   roundtable/deploy.sh
   ssh root@<droplet> systemctl restart nereus-roundtable
   ```

   `deploy.sh` reaches the server through the website's `nereus-web` alias
   (`NEREUS_RT_TARGET` points it elsewhere). It sends the server's code,
   its static files and the sample configuration, and nothing else.
5. Check it: `systemctl status nereus-roundtable`, `journalctl -u
   nereus-roundtable`, and `https://roundtable.nereussdr.com/` in a
   browser.

Updating is step 4 again. A change to a unit, the site file or the drop-in
is step 3 again.

## Backups

At 03:30 UTC the snapshot job writes a consistent copy of the database
(SQLite's online backup) to
`/var/lib/nereus-roundtable/backup/roundtable-YYYY-MM-DD.db` and keeps the
newest 2. While the disk is under 15% free it writes none and says so in
the journal (`journalctl -u nereus-roundtable-snapshot`).

JJ's machine pulls the newest each night with `deploy/pull-backup.sh`,
which checks the copy with SQLite's `integrity_check` and keeps the newest 7
in `~/NereusSDR-backups/roundtable`. It is a pull, so the server holds no
credentials for anything else. The snapshots sit in the server's private
state directory, which only root reads, so the pull logs in as root through
an alias in `~/.ssh/config`:

```
Host nereus-web-root
    HostName <droplet IPv4>
    User root
    IdentityFile <the key root accepts>
    IdentitiesOnly yes
```

Then, for example from `crontab -e`:

```
15 5 * * * /path/to/NereusSDR/roundtable/deploy/pull-backup.sh >> ~/NereusSDR-backups/pull.log 2>&1
```

`pull-backup.sh --dry-run` says what it would pull and remove. With 2 kept
on the server and 7 here, anything removed (text past a year, a removed
message, a deleted archive) is gone from every copy within 7 days.

To restore: stop the server, copy a snapshot over
`/var/lib/nereus-roundtable/roundtable.db` (removing any `-wal` and `-shm`
beside it), give it the folder's owner and mode 600 (`chown
--reference=/var/lib/nereus-roundtable /var/lib/nereus-roundtable/roundtable.db`,
`chmod 600` the same file), and start the server.

## Moving

Everything is the database and the configuration file. To move the server
to another host: stop the service, copy
`/var/lib/nereus-roundtable/roundtable.db` and
`/etc/nereus-roundtable/roundtable.conf` across, run `setup-server.sh` and
`deploy.sh` for the new host (its Caddy must import `/etc/caddy/sites/*.caddy`),
start the service once to make its state folder, stop it, copy the
database in as for a restore (above), start it again, and change the DNS
records. Stations and apps reach it
only by name, so none needs an update. A home host needs a fixed address
with TCP 443 forwarded, or a tunnel.
