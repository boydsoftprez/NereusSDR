#!/usr/bin/env python3
# =================================================================
# tools/digital-mode-opus-bench/run-bench.py  (NereusSDR)
# =================================================================
#
# no-port-check: NereusSDR-original. Remote daemon R3 receiver audio
# (R-R3-43, R-R3-23): how FT8, FST4 and Q65 decode through the Core's
# Opus settings compared with the untouched audio.
#
# For every mode, level, SNR step and signal:
#   1. a clean signal from the WSJT-X simulator (ft8sim, fst4sim, q65sim
#      with SNR 99, which writes the signal with no noise),
#   2. white Gaussian noise from a fixed seed, scaled by the simulators' own
#      SNR rule (signal power over noise power in 2500 Hz),
#   3. four arms decoded by jt9:
#        untouched  the 12 kHz 16-bit file as the simulator would write it
#        lossless   up to 48 kHz, the Core's L16 profile, back to 12 kHz
#        opus48     up to 48 kHz, the Core's Opus at 48 kbit/s, back
#        opus24     up to 48 kHz, the Core's Opus at 24 kbit/s, back
#      The codec stage is nereus-opus-bench-codec, which calls the Core's own
#      OpusAudioEncoder / OpusAudioDecoder and L16 packetiser.
#
# Files only; no audio device is opened. Decodes run a few at a time
# (--jobs, default 2) at lowered priority.
#
# Usage (from the repository root):
#   python3 tools/digital-mode-opus-bench/run-bench.py --build build-lane-b
#
# Each finished file's rows are appended to <out>/results.csv at once; a
# stopped run keeps them, and --summarise-only rebuilds results.md and
# summary.json from that file.
#
# =================================================================

import argparse
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import wave
from concurrent.futures import ProcessPoolExecutor, as_completed

import numpy as np

FS = 12000
UP = 4
ARMS = ["untouched", "lossless", "opus48", "opus24"]
PROFILES = ["lossless", "opus48", "opus24"]  # codec-tool profile names
NOISE_RMS_COUNTS = 100.0  # the simulators' noise: gran() * 100 counts
NOISE_BW_HZ = 2500.0      # WSJT-X SNR reference bandwidth

# Mode table. The target SNR range covers -10 to -24 dB at least and runs
# past each mode's decode limit so the weakest decode is inside the range.
MODES = {
    "ft8": {
        "seconds": 15, "snr": list(range(-10, -25, -1)),
        "decode": ["-8", "-d", "3"],
    },
    "fst4": {
        "seconds": 60, "snr": list(range(-10, -33, -1)),
        "decode": ["-7", "-p", "60", "-d", "3", "-f", "1500", "-F", "100"],
    },
    "q65": {
        "seconds": 60, "snr": list(range(-10, -33, -1)),
        "decode": ["-3", "-p", "60", "-b", "A", "-d", "3", "-f", "1500", "-F", "100"],
    },
}

# Levels of the noise at the codec input, dBFS (full scale = 1.0 float).
# -50.3 dBFS is what the simulators write (100 counts of 32768); -30 dBFS
# is a louder receiver output with the same SNRs.
LEVELS = {"sim": 20 * np.log10(NOISE_RMS_COUNTS / 32768.0), "loud": -30.0}

# Crowded FT8: the target signal plus ten strong or middling signals
# elsewhere in the passband, all in the same 15 s file.
CROWD_SNR = [10, 6, 3, 0, -3, -6, -8, -10, -12, -14]


def target_message(i):
    return "K%dA%sC W%dX%sZ %s" % (i % 10, chr(65 + i % 26), (i + 3) % 10,
                                   chr(65 + (i * 7) % 26),
                                   ["EN37", "FN42", "JO22", "IO91", "EM12"][i % 5])


def crowd_message(k):
    return "N%dB%sD KA%dY%sW %s" % (k % 10, chr(66 + k), (k + 5) % 10,
                                    chr(70 + k), ["DM79", "CN87", "EL98"][k % 3])


def target_freq(mode, i):
    # FT8: spread the targets over 300..2580 Hz; FST4 and Q65 sit at 1500 Hz
    # where their decoders look (-f 1500).
    return 300.0 + 120.0 * i if mode == "ft8" else 1500.0


def crowd_freqs(target):
    grid = [350.0 + 190.0 * k for k in range(14)]
    return [f for f in grid if abs(f - target) >= 120.0][:len(CROWD_SNR)]


# ---------------------------------------------------------------- signals

def read_wav(path):
    with wave.open(path) as w:
        assert w.getframerate() == FS and w.getnchannels() == 1 and w.getsampwidth() == 2
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)


def write_wav(path, counts):
    q = np.clip(np.rint(counts), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(q.tobytes())


def clean_signal(args, mode, message, freq):
    """Unit-amplitude clean signal at 12 kHz, cached under out/clean."""
    key = hashlib.sha1(("%s|%s|%.1f" % (mode, message, freq)).encode()).hexdigest()[:16]
    cache = os.path.join(args.out, "clean", mode, key + ".npy")
    if os.path.exists(cache):
        return np.load(cache)
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    work = tempfile.mkdtemp(prefix="sim-", dir=args.out)
    try:
        exe = os.path.join(args.wsjtx, mode + "sim")
        if mode == "ft8":
            cmd = [exe, message, "%.1f" % freq, "0.0", "0.0", "0.0", "1", "99"]
        elif mode == "fst4":
            cmd = [exe, message, "60", "%.1f" % freq, "0.0", "0.0", "0.0", "1", "99", "F"]
        else:
            cmd = [exe, message, "A", "%.1f" % freq, "0.0", "0.0", "0.0", "1", "60",
                   "1", "1", "99"]
        subprocess.run(cmd, cwd=work, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        wavs = [f for f in os.listdir(work) if f.endswith(".wav")]
        assert len(wavs) == 1, wavs
        x = read_wav(os.path.join(work, wavs[0]))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    peak = np.max(np.abs(x))
    assert peak > 30000, "simulator did not write a clean full-scale signal"
    x = x / peak
    np.save(cache, x)
    return x


def amplitude_for(snr_db, noise_rms):
    # The simulators: sig = sqrt(2 * 2500 / (fs / 2)) * 10^(snr / 20) for a
    # unit noise; a constant-envelope signal of amplitude A has power A^2 / 2.
    return noise_rms * np.sqrt(2.0 * NOISE_BW_HZ / (FS / 2.0)) * 10.0 ** (snr_db / 20.0)


# --------------------------------------------------------------- resample

def lowpass_taps():
    # Kaiser-windowed sinc at 48 kHz, cutoff 5.4 kHz (below 12 kHz Nyquist),
    # 385 taps: > 90 dB stopband past 6.6 kHz. Used for both directions.
    n = 385
    t = np.arange(n) - (n - 1) / 2.0
    fc = 5400.0 / (FS * UP)
    h = 2 * fc * np.sinc(2 * fc * t) * np.kaiser(n, 9.0)
    return h / np.sum(h)


TAPS = lowpass_taps()


def fftconv_same(x, h):
    n = len(x) + len(h) - 1
    size = 1 << (n - 1).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(h, size), size)[:n]
    d = (len(h) - 1) // 2
    return y[d:d + len(x)]


def up48(x12):
    z = np.zeros(len(x12) * UP)
    z[::UP] = x12
    return fftconv_same(z, TAPS * UP)


def down12(x48):
    return fftconv_same(x48, TAPS)[::UP]


# ----------------------------------------------------------------- decode

LINE = re.compile(r"^\s*\d+\s+(-?\d+)\s+(-?[\d.]+)\s+(\d+)\s+\S\s+(.*?)\s*$")


def decode(args, mode, wav_path, message):
    work = tempfile.mkdtemp(prefix="jt9-", dir=os.path.dirname(wav_path))
    try:
        res = subprocess.run([os.path.join(args.wsjtx, "jt9")] + MODES[mode]["decode"]
                             + [wav_path], cwd=work, capture_output=True, text=True,
                             timeout=600)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    found, snr, others = False, None, 0
    for line in res.stdout.splitlines():
        if line.startswith("<DecodeFinished>") or not line.strip():
            continue
        m = LINE.match(line)
        text = m.group(4) if m else line
        if message in text:
            if not found:
                found, snr = True, int(m.group(1)) if m else None
        else:
            others += 1
    return found, snr, others


# ------------------------------------------------------------------- jobs

def run_job(job):
    args, scenario, mode, level, snr, i = job
    try:
        os.nice(10)
    except OSError:
        pass
    seconds = MODES[mode]["seconds"]
    n = FS * seconds
    noise_rms = 10.0 ** (LEVELS[level] / 20.0) * 32768.0  # in 16-bit counts
    message = target_message(i)
    freq = target_freq(mode, i)

    def fit(x):
        y = np.zeros(n)
        y[:min(n, len(x))] = x[:n]
        return y

    x = amplitude_for(snr, noise_rms) * fit(clean_signal(args, mode, message, freq))
    expected = [message]
    if scenario == "crowded":
        for k, (f, s) in enumerate(zip(crowd_freqs(freq), CROWD_SNR)):
            x += amplitude_for(s, noise_rms) * fit(clean_signal(args, mode, crowd_message(k), f))
    seed = [args.seed, list(MODES).index(mode), list(LEVELS).index(level),
            ["single", "crowded"].index(scenario), snr + 1000, i]
    rng = np.random.default_rng(seed)
    x += noise_rms * rng.standard_normal(n)

    work = tempfile.mkdtemp(prefix="job-", dir=args.out)
    rows = []
    try:
        untouched = os.path.join(work, "untouched.wav")
        write_wav(untouched, x)
        x12 = read_wav(untouched)  # exactly the 16-bit file WSJT-X would read
        f48 = os.path.join(work, "in48.f32")
        up48(x12 / 32768.0).astype("<f4").tofile(f48)
        wavs = {"untouched": untouched}
        for profile in PROFILES:
            out48 = os.path.join(work, profile + ".f32")
            subprocess.run([args.codec, profile, f48, out48], check=True)
            y = down12(np.fromfile(out48, dtype="<f4").astype(np.float64))
            wavs[profile] = os.path.join(work, profile + ".wav")
            write_wav(wavs[profile], y * 32768.0)
        for arm in ARMS:
            found, dsnr, others = decode(args, mode, wavs[arm], message)
            rows.append({"scenario": scenario, "mode": mode, "level": level, "snr": snr,
                         "signal": i, "arm": arm, "decoded": int(found),
                         "reported_snr": "" if dsnr is None else dsnr,
                         "other_decodes": others})
        if args.keep_audio:
            keep = os.path.join(args.out, "audio", scenario, mode, level, str(snr))
            os.makedirs(keep, exist_ok=True)
            for arm in ARMS:
                shutil.copy(wavs[arm], os.path.join(keep, "%02d-%s.wav" % (i, arm)))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return rows


# ---------------------------------------------------------------- summary

FIELDS = ["scenario", "mode", "level", "snr", "signal", "arm", "decoded",
          "reported_snr", "other_decodes"]


def read_results(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            for k in ("snr", "signal", "decoded", "other_decodes"):
                r[k] = int(r[k])
            rows.append(r)
    return rows


def summarise(rows):
    """Tables per scenario, mode and level. Each step's denominator is the
    number of files finished at that step, so a stopped run still reads."""
    groups = {}
    for r in rows:
        groups.setdefault((r["scenario"], r["mode"], r["level"]), []).append(r)
    lines = []
    summary = {}
    for (scenario, mode, level), rs in sorted(groups.items()):
        snrs = sorted({r["snr"] for r in rs}, reverse=True)
        files = {s: len({r["signal"] for r in rs if r["snr"] == s}) for s in snrs}
        table = {arm: {s: sum(r["decoded"] for r in rs if r["arm"] == arm and r["snr"] == s)
                       for s in snrs} for arm in ARMS}
        weakest = {arm: min([s for s in snrs if table[arm][s] > 0], default=None)
                   for arm in ARMS}
        totals = {arm: sum(table[arm].values()) for arm in ARMS}
        # SNR where half the signals decode, by linear interpolation
        half = {}
        for arm in ARMS:
            h = None
            for a, b in zip(snrs, snrs[1:]):
                pa, pb = table[arm][a] / files[a], table[arm][b] / files[b]
                if pa >= 0.5 > pb:
                    h = a + (b - a) * (pa - 0.5) / (pa - pb)
                    break
            half[arm] = h
        others = {arm: sum(r["other_decodes"] for r in rs if r["arm"] == arm) for arm in ARMS}
        key = "%s/%s/%s" % (scenario, mode, level)
        summary[key] = {"files": files, "decodes": table, "weakest": weakest,
                        "total": totals, "half_point_db": half, "other_decodes": others}
        lines.append("### %s, %s, noise at %.1f dBFS" % (
            mode.upper() + (" crowded" if scenario == "crowded" else ""), level, LEVELS[level]))
        lines.append("")
        lines.append("| SNR (dB) | " + " | ".join(ARMS) + " |")
        lines.append("|---:|" + "---:|" * len(ARMS))
        for s in snrs:
            lines.append("| %d | " % s + " | ".join("%d/%d" % (table[a][s], files[s])
                                                     for a in ARMS) + " |")
        lines.append("| total | " + " | ".join("%d" % totals[a] for a in ARMS) + " |")
        lines.append("| weakest decoded (dB) | " + " | ".join(
            "none" if weakest[a] is None else "%d" % weakest[a] for a in ARMS) + " |")
        lines.append("| 50 % point (dB) | " + " | ".join(
            "n/a" if half[a] is None else "%.1f" % half[a] for a in ARMS) + " |")
        lines.append("| other decodes | " + " | ".join("%d" % others[a] for a in ARMS) + " |")
        lines.append("")
    return "\n".join(lines), summary


def write_reports(args, rows, describe, version):
    text, summary = summarise(rows)
    header = ("Codec profiles (nereus-opus-bench-codec --describe):\n\n```\n%s```\n\n"
              "Decoder: %s jt9; seed %d.\n\n" % (describe, version, args.seed))
    with open(os.path.join(args.out, "results.md"), "w") as f:
        f.write(header + text)
    with open(os.path.join(args.out, "summary.json"), "w") as f:
        json.dump({"seed": args.seed, "wsjtx": version, "codec": describe,
                   "results": summary}, f, indent=1, default=str)
    print(header + text)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", ".."))
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", default="build", help="CMake build directory (for the codec tool)")
    p.add_argument("--no-build", action="store_true", help="do not build the codec tool first")
    p.add_argument("--out", default=None, help="work and results directory "
                   "(default <build>/digital-mode-opus-bench)")
    p.add_argument("--wsjtx", default="/Applications/wsjtx.app/Contents/MacOS")
    p.add_argument("--modes", default="ft8,fst4,q65")
    p.add_argument("--count", type=int, default=20, help="signals per SNR step")
    p.add_argument("--seed", type=int, default=20260923)
    p.add_argument("--jobs", type=int, default=2, help="files decoded at once")
    p.add_argument("--keep-audio", action="store_true")
    p.add_argument("--summarise-only", action="store_true",
                   help="build results.md and summary.json from an existing results.csv")
    args = p.parse_args()

    build = os.path.join(root, args.build) if not os.path.isabs(args.build) else args.build
    args.out = os.path.abspath(args.out or os.path.join(build, "digital-mode-opus-bench"))
    args.codec = os.path.join(build, "nereus-opus-bench-codec")
    os.makedirs(args.out, exist_ok=True)
    if not args.no_build and not args.summarise_only:
        subprocess.run(["cmake", "--build", build, "--target", "nereus-opus-bench-codec"],
                       check=True)
    describe = subprocess.run([args.codec, "--describe"], check=True, capture_output=True,
                              text=True).stdout
    version = subprocess.run([os.path.join(args.wsjtx, "wsjtx_app_version"), "-v"],
                             capture_output=True, text=True).stdout.strip()

    results = os.path.join(args.out, "results.csv")
    if args.summarise_only:
        write_reports(args, read_results(results), describe, version)
        return 0

    modes = args.modes.split(",")
    jobs = []
    for mode in modes:
        for level in (LEVELS if mode == "ft8" else ["sim"]):
            for snr in MODES[mode]["snr"]:
                for i in range(args.count):
                    jobs.append((args, "single", mode, level, snr, i))
    if "ft8" in modes:
        for snr in MODES["ft8"]["snr"]:
            for i in range(args.count):
                jobs.append((args, "crowded", "ft8", "sim", snr, i))

    # Clean signals first, one at a time, so workers only read the cache.
    for (_, scenario, mode, level, snr, i) in jobs:
        freq = target_freq(mode, i)
        clean_signal(args, mode, target_message(i), freq)
        if scenario == "crowded":
            for k, f in enumerate(crowd_freqs(freq)):
                clean_signal(args, mode, crowd_message(k), f)

    # Every finished file's rows go to results.csv at once, so a stopped run
    # keeps everything it finished (--summarise-only reads it back).
    start = time.time()
    rows = []
    with open(results, "w", newline="") as out, \
            ProcessPoolExecutor(max_workers=args.jobs) as pool:
        writer = csv.DictWriter(out, fieldnames=FIELDS)
        writer.writeheader()
        out.flush()
        futures = [pool.submit(run_job, job) for job in jobs]
        for n, future in enumerate(as_completed(futures), 1):
            result = future.result()
            writer.writerows(result)
            out.flush()
            rows.extend(result)
            if n % 25 == 0 or n == len(jobs):
                print("%d/%d files, %.0f s" % (n, len(jobs), time.time() - start), flush=True)

    write_reports(args, rows, describe, version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
