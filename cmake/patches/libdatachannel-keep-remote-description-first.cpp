/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 * Copyright (c) 2020 Filip Klembara (in2core)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * The notice above is libdatachannel src/peerconnection.cpp's own, copied
 * byte for byte from v0.24.5 (MPL-2.0 Exhibit A). This change becomes part
 * of that covered file, so under MPL-2.0 section 3.1 it is licensed under
 * the Mozilla Public License 2.0, not under NereusSDR's GPLv3.
 *
 * NereusSDR change to libdatachannel v0.24.5 src/peerconnection.cpp
 * (MPL-2.0), PeerConnection::setRemoteDescription(), put in place of the
 * two lines it reorders by cmake/NereusRemoteMedia.cmake in a copy of the
 * file in the build tree; the fetched source is not modified. R-R3-49,
 * 2026-09-27, J.J. Boyd (KG4VCF), with AI-assisted implementation via
 * Anthropic Claude Code.
 *
 * The remote description is kept (processRemoteDescription) before the
 * ICE agent is given it. v0.24.5 did the reverse: from the moment the
 * agent has the remote credentials it can complete connectivity checks
 * the far end is already sending, and the DTLS handshake that follows
 * checks the peer's certificate against the kept description's
 * fingerprint (impl/peerconnection.cpp, checkFingerprint), which fails
 * while none is kept yet ("DTLS alert: unknown CA"). An offerer taking
 * its answer on a busy computer lost that race and its connection
 * failed. RFC 5763 section 5 expects the handshake to run alongside the
 * answer; with the fingerprint kept first it can.
 */
	impl()->processRemoteDescription(description);
	PLOG_DEBUG << "Remote description kept before the ICE agent takes it";

	iceTransport->setRemoteDescription(description); // ICE transport might reject the description

