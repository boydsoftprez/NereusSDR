# WDSP 2.10 vendor inventory

This inventory fixes the source baseline and merge dispositions for the WDSP 2.10 import. It compares the Nereus tree at `66cb0eb883b9fc9a4e1abb21f0a1b0a991458307` with TAPR/OpenHPSDR-wdsp commit `b02d5bac675dd2f33ec2bab2b339f79a597c47dd`, directory `wdsp 2.10/Source`. The pinned checkout used for the audit is `/tmp/nereus-wdsp210-upstream.PRKvM3/repo`; it can be reproduced with:

```sh
git clone https://github.com/TAPR/OpenHPSDR-wdsp.git
git -C OpenHPSDR-wdsp checkout b02d5bac675dd2f33ec2bab2b339f79a597c47dd
```

[`source-manifest.csv`](source-manifest.csv) is the machine-readable inventory. Its 170 rows are the case-preserving union of every `.c` and `.h` file in the two source directories. Every row contains both available SHA-256 hashes, the relationship, whether the file is compiled now and after import on non-Windows hosts, and its disposition. The manifest was independently recomputed with zero hash mismatches.

## Compile inventory and dispositions

The current `third_party/wdsp/CMakeLists.txt` glob compiles all 72 current `.c` files on non-Windows hosts and 71 on Windows, where it excludes `linux_port.c`. All 72 are classified below.

| Relationship and disposition | Count | Current compiled files |
| --- | ---: | --- |
| Identical to pinned; use the pinned copy verbatim | 32 | `ammod.c`, `amsq.c`, `analyzer.c`, `apfshadow.c`, `channel.c`, `cmath.c`, `compress.c`, `delay.c`, `div.c`, `eer.c`, `gain.c`, `gen.c`, `impulse_cache.c`, `iobuffs.c`, `lmath.c`, `main.c`, `meter.c`, `meterlog10.c`, `nob.c`, `nobII.c`, `osctrl.c`, `resample.c`, `rmatch.c`, `sender.c`, `shift.c`, `siphon.c`, `slew.c`, `ssql.c`, `syncbuffs.c`, `varsamp.c`, `wcpAGC.c`, `zetaHat.c` |
| Modified common file; replace with pinned | 26 | `TXA.c`, `bandpass.c`, `calcc.c`, `calculus.c`, `cblock.c`, `cfir.c`, `dexp.c`, `doublepole.c`, `emph.c`, `eq.c`, `fcurve.c`, `fir.c`, `firmin.c`, `fmd.c`, `fmmod.c`, `fmsq.c`, `gaussian.c`, `icfir.c`, `iir.c`, `iqc.c`, `matchedCW.c`, `nbp.c`, `patchpanel.c`, `utilities.c`, `version.c`, `wisdom.c` |
| Modified common file; start from pinned and restore NR3/NR4 hooks | 6 | `RXA.c`, `amd.c`, `anf.c`, `anr.c`, `emnr.c`, `snb.c` |
| Modified common file; start from pinned and add the CFC Q compatibility layer | 1 | `cfcomp.c` |
| Nereus-only portability file; retain and extend | 1 | `linux_port.c` |
| Nereus-only NR3/NR4 extension; retain and integrate | 2 | `rnnr.c`, `sbnr.c` |
| Nereus-only compatibility glue still called by C++ | 3 | `netinterface_stub.c`, `ps_sync_stub.c`, `txgain_stub.c` |
| Nereus-only obsolete implementation; remove | 1 | `FDnoiseIQ.c` |

The corresponding header decisions are in the manifest. In particular, `RXA.h` and `comm.h` require an additive NR3/NR4 merge; `cfcomp.h` requires the Q compatibility merge; `linux_port.h`, `rnnr.h`, and `sbnr.h` remain local extensions; and `FDnoiseIQ.h` and the unreferenced `fastmath.h` are removed.

Pinned 2.10 adds 13 `.c` files: `extrapolate.c`, `nnet.c`, `nnio.c`, `nnr.c`, `nnr_model_0.c`, `nnr_model_1.c`, `nurbs.c`, `nurbs_fit.c`, `nurbs_spline.c`, `phrot.c`, `reshb.c`, `snoop.c`, and `wbfm.c`. Import all except `snoop.c`. `snoop.c` has no public header, its declaration and call in `TXA.c` are commented out, and its implicit-int `void xsnoop(channel)` definition does not compile as C11. It is dormant debugging code rather than a runtime dependency.

After import, the intended non-Windows library has 83 `.c` files: 77 pinned files after excluding `snoop.c`, plus retained `linux_port.c`, `rnnr.c`, `sbnr.c`, and the three stubs. Windows has 82 after excluding `linux_port.c`. Replace the source glob with an explicit list so a dormant or newly added upstream source cannot silently enter the library.

The new dependency chains are:

- RX NNR: `RXA` -> `nnr` -> `nnet` -> `nnio` plus `nnr_model_0` and `nnr_model_1`.
- PS3 correction: `calcc` -> `extrapolate`, `nurbs_fit`, and `nurbs_spline`; `cfcomp` and `eq` use `nurbs`.
- RX input: pinned `RXA` uses `wbfm`, which uses the `reshb` half-band resampler.
- TX: pinned `TXA` uses `phrot`.

`FDnoiseIQ` has no consumer after pinned `emnr.c` replaces its table with the newer generated phase/RNG path. `fastmath.h` has no current consumer. The three stubs still satisfy application call sites. `rnnr` and `sbnr` require a deliberate merge into pinned RXA creation, destruction, execution, mode/rate changes, and bandpass arbitration. The merged `RXAbp1Check` inputs must cover AMD, SNBA, EMNR, NNR, ANF, ANR, RNNR, and SBNR; NNR and retained-mode run setters must also trigger the check.

## POSIX compile audit

A scratch-only Apple Clang C11 probe compiled all 78 intended units in the probed tree: the 77 pinned sources after excluding `snoop.c`, plus `linux_port.c`. No production file was changed for the probe. It established these required port changes:

1. PS3 `calcc.c` needs `CreateSemaphoreW`, `WaitForMultipleObjects`, `WAIT_OBJECT_0`, `WAIT_TIMEOUT`, and `WAIT_FAILED`. The POSIX wait-any implementation must reject wait-all, retry `EINTR`, use a monotonic finite deadline, and rotate its scan start to avoid starving later handles.
2. `extrapolate.c`, `nurbs_fit.c`, and `nurbs_spline.c` call `_aligned_malloc`/`_aligned_free` without including `comm.h`. Give those translation units explicit visibility of the portability definitions. The target ABIs already give `malloc` the requested 16-byte fundamental alignment.
3. WDSP's internal `dprintf(const char*, ...)` conflicts with the POSIX `dprintf(int, ...)`; rename or macro-map the WDSP helper on POSIX.
4. Add the missing POSIX `InitializeCriticalSection` mapping used by `eq.c` and an `OutputDebugStringA` stderr/no-op mapping used by `utilities.c`.
5. `wbfm.c` requires `<limits.h>` for `INT_MAX`.
6. Keep `snoop.c` out of the explicit source list.

`nnet.c` already uses `clock_gettime(CLOCK_MONOTONIC)` on POSIX. The three retained stub translation units also compiled against the scratch pinned headers. `rnnr.c` and `sbnr.c` intentionally did not compile against an unmerged pinned `RXA.h`, which confirms that their lifecycle integration cannot be an accidental overlay. This was a compile inventory, not a linked or runtime claim.

## Public-symbol and ABI audit

The current application-facing header `src/core/wdsp_api.h` declares 268 distinct functions. All 268 names exist as global definitions in the baseline `libwdsp_static.a`. The pinned `wdsp.h` declares 540 functions: 250 names overlap, 290 are pinned-only, and 18 are current-only. The larger pinned catalogue includes internal/low-level entry points and is an API reference; it should not replace the curated host header wholesale.

The 18 current-only names have explicit dispositions:

- Preserve the NR3/NR4 APIs: `RNNRloadModel`, `SetRXARNNRPosition`, `SetRXARNNRRun`, `SetRXARNNRUseDefaultGain`, `SetRXASBNRRun`, `SetRXASBNRnoiseRescale`, `SetRXASBNRnoiseScalingType`, `SetRXASBNRpostFilterThreshold`, `SetRXASBNRreductionAmount`, `SetRXASBNRsmoothingFactor`, and `SetRXASBNRwhiteningFactor`.
- Remove the five PS2 controls eliminated by PS3: `SetPSPinMode`, `SetPSMapMode`, `SetPSStabilize`, `SetPSPtol`, and `SetPSIntsAndSpi`.
- Retain the Nereus routing glue `SetPSRxIdx` and `SetPSTxIdx` while its current application callers remain.

The ABI-significant same-name changes are:

- `GetPSDisp` changes from seven output arrays to four sample arrays, four fixed correction arrays, and three scalar outputs: `nsamps_out`, `cpts_out`, and `phs_ref_deg_out`. Callers must preallocate for 4096 raw samples and 512 correction points before entering this capacity-less C API.
- Pinned `SetTXACFCOMPprofile(channel,n,F,G,E)` conflicts with the established Nereus `SetTXACFCOMPprofile(channel,n,F,G,E,Qg,Qe)`. Keep the seven-argument Nereus symbol and add pinned split-profile/curve APIs under their upstream names.
- The remaining parser-reported differences are type-alias, parameter-name, or platform calling-convention spelling (`INREAL`/`OUTREAL`, analyzer parameter names, and `__stdcall` versus `NEREUS_STDCALL`); they do not change the target ABI.

New curated host declarations are required for the NNR family (`SetNNRModelPathSlot`, `SetNNRModelPath`, `SetRXANNRRun`, `SetRXANNRPosition`, `SetRXANNRMaskFloor`, model set/get, test/layout modes, alpha/knee/tau/max-gain/smoothing) and CFC split-profile, curve, weight, and draw functions. `SetRXANNRModel` returns acceptance; callers must use that result rather than assuming a requested model became active.

## CFC compatibility contract

Pinned CFC independently stores the G and E frequency profiles and adds NURBS degrees/weights. The compatibility implementation must preserve the actual current construction defaults and the existing seven-argument Q behavior while retaining the pinned split APIs. The existing algorithm clamps Q to `0.01` and uses `TAIL_MIX=0.08`, `TAIL_SCALE=2.5`, `BW_REF_HZ=1000.0`, `MIN_SIGMA=1e-12`, `FWHM_TO_SIGMA=1/sqrt(2*log(2))`, and a two-FFT-bin minimum FWHM. `Qg` and `Qe` remain independently optional; the G setter invalidates only G state and the E setter only E state.

The frozen linked-WDSP reference is `tests/WdspCfcReference.h`: 65 compression/equalization bin pairs at `1e-11` tolerance for the constructor default and four independent Q combinations (both arrays, G only, E only, neither). This fixture is the acceptance oracle for the compatibility merge, including constructor defaults.

## PS3 correction geometry

Pinned `nurbs_spline.h` defines eight samples per control point, and `calcc.c` caps a fit at 200 control points. The maximum sampled curve therefore has 1600 points. `find_branches` has storage for 16 branches but stops after emitting at most 15. Adjacent branches share one source point, so the maximum aggregate number stored by the current writer is:

```text
1600 sampled points + (15 branches - 1 shared-boundary duplicates) = 1614 points
```

Use **1614 points per curve** as the tight cap for the pinned writer. The earlier estimate of 1616 is safe but not exact. `PS_NS_EXTEND_LEFT_MODE` is currently zero. Enabling it can add 16 net points to one curve, raising the bound to 1630, and must force a cap review. Each of MAG, COS, and SIN has seven metadata lines, 256 EMA values, then its spline branches. Keep the 1 MiB whole-file ceiling; it is comfortably above maximum writer output.

The pinned reader validates parsing, version/curve tags, the 256-value EMA count, and checksum, but does not fully validate branch/point counts, key uniqueness, finite values, x ordering/ranges, or trailing content before allocation. Host preflight and the vendor reader must validate those properties.

## Embedded NNR model bounds

Both embedded assets are valid `WDSPNN\0\0` version-1 payloads with 57 non-overlapping F64 tensors and a 4160-byte data offset.

| Asset | C source bytes / SHA-256 | Embedded bytes / SHA-256 | Elements / decoded bytes | Largest tensor |
| --- | --- | --- | --- | --- |
| `nnr_model_0.c` | 10,758,642 / `af9174a63cd0683efed03feb97d45419ca52a881943c25153299b02aed607a98` | 2,098,944 / `e1ebfed6f522746bcfc1265d4990f0250ae1de5b2050e58af9fc965dc1d94c96` | 261,842 / 2,094,736 | `dec1_w`, `(128,48,2,5)`, 491,520 bytes |
| `nnr_model_1.c` | 23,998,035 / `62593d935c5a7b4970fac988b76aa28d038e97f7b56a48480a3be7346749a331` | 4,682,240 / `925fdb6830627d84ef4ec8b2116ad66047e56d593d168db10f510dcbac0934a1` | 584,754 / 4,678,032 | `dec1_w`, `(192,72,2,5)`, 1,105,920 bytes |

The parser supports F32 and F64 tensors even though the embedded assets are all F64. Under the approved 64 MiB encoded-file ceiling, a valid F32 file can describe at most 16,777,216 elements and expand to 128 MiB of decoded doubles, in addition to the encoded buffer. Preflight must use checked 64-bit arithmetic for dimensions, element products, byte products, offsets, ranges, and aggregate decoded allocation; require 1-4 positive dimensions; require the product times dtype width to equal the descriptor `nbytes`; keep every descriptor in `data_bytes`; and reject overlapping descriptors. The current signed-`int` `numel` and total calculations are not safe for hostile assets.

## Verification status and remaining uncertainty

At the pre-import commit, the configured DFNR/MNR build succeeded and all 11 focused NR/PS/CFC baseline tests passed. The four-profile linked CFC capture also passed before its reference was frozen. The source manifest hashes, compile counts, C11 probe, public-header-to-archive symbol match, model decoding, and spline bound were independently checked.

## Post-import disposition and compile evidence

The implemented non-Windows source list contains exactly the 83 reviewed C translation units and no unreviewed glob additions. It contains 77 pinned units (all pinned C files except dormant `snoop.c`) and six retained Nereus units: `linux_port.c`, `netinterface_stub.c`, `ps_sync_stub.c`, `rnnr.c`, `sbnr.c`, and `txgain_stub.c`. `FDnoiseIQ.c/.h` and `fastmath.h` are absent. Windows removes only `linux_port.c` from the explicit list.

Fifty-seven pinned C files remain byte-identical after import. Twenty pinned C files carry reviewed compatibility or hardening changes:

- `RXA.c`, `amd.c`, `anf.c`, `anr.c`, `emnr.c`, and `snb.c` compose the retained NR3/NR4 modes with the pinned NNR lifecycle and bandpass arbitration.
- `TXA.c` preserves the measured 2048-point CFC construction geometry.
- `cfcomp.c` preserves the seven-argument independent-Q compatibility behavior alongside the pinned split linear/NURBS APIs.
- `cfir.c` and `emph.c` provide null-safe run readbacks for the now-opaque pinned filter handles used by host stage diagnostics. `emph.c` and `fmd.c` also retain the replacement filter-curve object returned during coefficient-count changes; pinned `b02d5bac` freed the prior object but discarded the replacement, leaving the resize path to dereference freed storage.
- `calcc.c` and `iqc.c` add bounded PS3 display access, opaque state readbacks, asynchronous file-operation status, durable worker/transition cancellation before teardown, and explicit active-stream, quiescent, and retained-correction lifecycle operations. A correction epoch plus a pre-published suspension flag prevents a calculation or restore worker that was already in flight from re-arming IQC after stop.
- `nnet.c`, `nnio.c`, and `nnr.c` add source/status readback, partial-construction cleanup, checked model-table arithmetic, allocation bounds, descriptor validation, and the accepted-config/status adapter boundary.
- `nurbs_spline.c` adds bounded correction-file parsing. It accepts the writer/storage grammar of 1..16 branches, either zero points or at least four points per branch, at most 1614 aggregate points, finite normalized `t_mid`, and finite strictly increasing branch x coordinates. It does not impose guessed x/y value ranges.
- `extrapolate.c`, `nurbs_fit.c`, and `wbfm.c` carry the audited POSIX declaration/include fixes.

Ten pinned headers carry the matching additive interfaces or public-header portability fix: `RXA.h`, `calcc.h`, `cfcomp.h`, `cfir.h`, `comm.h`, `emph.h`, `iqc.h`, `nnet.h`, `nnr.h`, and `resample.h`. The retained/new local public headers are `linux_port.h`, `rnnr.h`, `sbnr.h`, `nnr_compat.h`, and `ps3_abi.h`; the monolithic pinned `wdsp.h` remains intentionally absent.

The narrow PS3 ABI publishes `PS3_MAX_DISPLAY_SAMPLES=4096`, `PS3_DISPLAY_CORRECTION_POINTS=512`, and these eight functions: `GetPSRunCal`, `GetPSCorrectionState`, `RequestPSCorrectionStop`, `StopPSCorrectionQuiescent`, `ApplyPSCorrection`, `GetPSCorrectionAvailable`, `GetPSFileOperationStatus`, and `CancelPSFileOperation`. Each name has one declaration in `ps3_abi.h` and one source definition in `calcc.c` or `iqc.c`.

`GetPSCorrectionState` copies the IQC run bit and its BEGIN/SWAP/END transition-in-progress `busy` latch while holding the channel DSP lock. `RequestPSCorrectionStop` is the nonblocking active-stream path: it retires CALCC to `LRESET`, requests the normal IQC `END` ramp, and leaves callers to observe `run=0,busy=0`. `StopPSCorrectionQuiescent` is the immediate path and requires the host to have established that TXA sample processing is stopped; it sets IQC to `DONE` with `run=0,busy=0` while retaining the current curves. `ApplyPSCorrection` refuses a fresh or incomplete curve set and re-enters IQC through `BEGIN`, preserving the normal ramp when samples resume. `GetPSCorrectionAvailable` reports whether all three retained current-set curves exist.

Stop publishes correction suspension before waiting for the CALCC update lock, allowing a calculation worker blocked on an undrainable IQC transition to leave its wait. The stop epoch is captured when calculation starts and checked again under the update lock before IQC installation, so a later restore or apply cannot make an old calculation current. A restore worker also rechecks its file epoch/cancellation under that lock before publishing CALCC running state. File-operation generations advance once on every accepted operation completion, synchronous path error, or teardown cancellation; `info[12]` remains only the legacy latest-error bits and is not a completion token.

The merged archive compiled successfully on Apple Clang/arm64 in 91 build actions with no vendor source warnings or errors. Its global symbol table contains the initial opaque PS3 readbacks and accepted NNR adapter entry points. The final archive export census found all 287 parsed curated/compatibility API declarations, including all eight active/quiescent stop, retained-correction and file/status helpers, with zero missing symbols. Source-level reproduction of the frozen CFC inputs matched all tested linear and independent-Q outputs with maximum absolute error `2.22e-16`, well inside the frozen linked-test tolerance.

On Apple Clang/arm64 macOS the application subsequently compiled, and the linked `tst_linux_port_wait`, `tst_wdsp_ps_smoke`, `tst_tx_channel_ps_setters`, and `tst_wdsp210_cfc_compat` targets passed. The real linked NNR target passed in 34.78 seconds. No pinned PS3 correction-file fixture was found, so correction-parser tests generate valid writer-shaped fixtures and malformed variants.

The final `tst_wdsp210_cfc_compat` source also exercises both real public filter-resize collectives (`RXASetNC` and `TXASetNC`), processes their configured RX/TX block geometry, performs a second resize, and lets normal channel teardown release the final objects. This regression passed in the final combined build with the frozen 65-pair CFC fixture unchanged; the full 716-target suite also passed.

Remaining platform verification includes Linux and Windows builds, repeated PS3 creation/teardown with pending work on those platforms, and hardware/RF acceptance. Native AddressSanitizer scope and results are recorded in the [execution ledger](README.md). Final source hashes confirm 126 byte-identical pinned files, 30 modified pinned files and 11 local files. None of the macOS software results imply the outstanding platform or RF results.
