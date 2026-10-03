# Unexpected media connection recovery

Requirement: R-R3-28. This is a bounded GUI/Core lifecycle correction, using
`yonder-cost-aware-execution`; it does not implement R5 traversal or alter
Opus buffering, radio stream recovery or network configuration.

## Live defect and chosen boundary

At installed checkpoint `8c011066`, a media connection closed at 17:39:01 while
the authenticated Core control session remained alive. RemoteMediaController
stopped its peer and presented a warning, but nothing requested another session.
The connection panel still reported Core connected. Manual Disconnect/Connect
restored receive. A later typed backend peer failure exposed the same missing
recovery path before the control heartbeat eventually timed out.

Recovery uses the existing StationClient reconnect path to retire both ends
and negotiate a fresh authenticated epoch. This retains its saved endpoint,
certificate pin, token, cancellation and authoritative snapshot behavior. A new
media-only replacement protocol is unnecessary for this correction and would
need additional coordination with a daemon that can still own the old peer.

Connectivity failure has a dedicated signal from the transport, through
MediaPeer to the GUI. Generic packet/SDP/candidate, decoder and audio-device
errors remain separate. An unexpected current-peer close or typed failure
requests recovery at most once. The queued connection-controller handler must
still see the same authenticated epoch and no operator Disconnect before
requesting existing backed-off reconnect. Old peers and intentional shutdowns
cannot revive a stopped or replacement session.

## Regression evidence

The first peer regression compiled, then failed with zero relayed terminal
failures against one expected; seven other cases passed. After that relay was
implemented, the combined run passed the peer suite but reproduced both missing
GUI/control behaviors: no recovery request after an established media close,
and no scheduled retry through the queued controller slot. The other 16 GUI/
control cases passed; no case skipped. Private logs:
`r3-media-recovery-peer-red-{build,test}.log` and
`r3-media-recovery-controls-red-{build,test}.log`.

After implementation, a fresh focused build/run of `tst_media_peer`,
`tst_remote_media_controller` and `tst_remote_connection_controls` passed
**3/3 executables in 9.72 seconds, zero Qt case skips**. The integration case
now drives an actual fake transport close through the production signal path
into a real pinned TLS reconnect, preserving one slice, its pointer and its
frequency. Additional cases cover pre-ready typed failure, duplicate recovery,
stale epochs, manual cancellation and synchronous diagnostic-listener deletion.
Logs: `r3-media-recovery-green-{build,test,cases}.log`.

One consolidated independent review found no actionable correctness,
lifecycle, authentication or regression-coverage issues. Full-suite software
verification passed as recorded below. Installation is now complete as recorded below; software evidence alone does
not establish live media-only recovery.

## Explicit remaining boundary

A typed connection failure before media readiness can also enter the existing
retry path. However, each successful Core handshake resets its reconnect delay,
so repeated media negotiation failures after successful handshakes remain
limited to the first retry delay, rather than growing across media failures.
Initial backend start refusal and negotiation that never reaches a terminal
state need their own deadline/backoff acceptance. Keep that follow-up visible;
do not claim this correction completes all media establishment recovery.

## Full-suite validation follow-up

The initial fresh unfiltered run passed 681/682 executables. The sole failure
was the unrelated RF2K-S command test checking for a request after a fixed
200 ms delay. Its four command cases now wait for the exact expected request
with a bounded QTRY assertion, retaining the payload assertions and production
behavior. Three consecutive focused runs passed in 2.32 seconds total.
The final fresh `all_tests`/`nereusd` build and unfiltered ctest run passed
**682/682 executables in 168.82 seconds**, with zero failures or executable
skips. Eleven pre-existing inner Qt cases skipped for unavailable host APIs
or deferred harness coverage; all new recovery cases executed. Private logs:
`r3-media-recovery-final-{build,test,cases}.log`. The earlier failed run is
retained separately. No production amplifier behavior changed.

## Installed checkpoint 706b9a5f

Native and macOS builds, source-hash validation, library/symbol checks,
rollback-protected install and GUI code-signature verification passed. During
installation the already-running new GUI observed Core shutdown, retried,
and resumed an authenticated session, audio and display at 18:31:25. This is
live full-Core restart recovery; a media-only failure with surviving control
has not yet been reproduced on the installed build. Preserve that distinction.
The [deployment ledger](README.md) contains hashes and remaining acceptance.

The operator reported both audio and waterfall smooth at this checkpoint.
A later log check over 18:31:25–19:00:59 found six audio-buffer underflow
warnings and 171 RTP timing warnings; Core remained PID 3656 with zero service
restarts. This is useful short receive evidence, not acceptance of the
two-hour audio soak or proof that occasional stutters are resolved.
