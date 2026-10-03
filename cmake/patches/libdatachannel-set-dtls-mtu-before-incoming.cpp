/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * The notice above is libdatachannel src/impl/dtlstransport.cpp's own,
 * copied byte for byte from v0.24.5 (MPL-2.0 Exhibit A). This change becomes
 * part of that covered file, so under MPL-2.0 section 3.1 it is licensed
 * under the Mozilla Public License 2.0, not under NereusSDR's GPLv3.
 *
 * NereusSDR change to libdatachannel v0.24.5 src/impl/dtlstransport.cpp
 * (MPL-2.0), the OpenSSL DtlsTransport::start(), put in place of its first
 * lines by cmake/NereusRemoteMedia.cmake in a copy of the file in the build
 * tree; the fetched source is not modified. R-R3-49, 2026-09-27, J.J. Boyd
 * (KG4VCF), with AI-assisted implementation via Anthropic Claude Code.
 *
 * The DTLS MTU is set before incoming records are taken. v0.24.5 registered
 * the incoming callback and entered Connecting first, and set the MTU only
 * afterwards. A peer's ClientHello arriving in between was handed to
 * doRecv() on the thread pool, which ran the handshake with no MTU set.
 * With SSL_OP_NO_QUERY_MTU OpenSSL cannot find one (dtls1_query_mtu returns
 * 0, so dtls1_do_write fails with nothing on the error queue), the server
 * flight is never written, and the transport failed with "DTLS recv:
 * Handshake failed: fatal I/O error": "the data channel did not open", or
 * "media peer connection failed" on the media peer. The answering side
 * lost this race whenever its ICE connected just after the offering side's.
 */
	PLOG_DEBUG << "Starting DTLS transport";
	{
		std::lock_guard lock(mSslMutex);

		size_t mtu = mMtu.value_or(DEFAULT_MTU) - 8 - 40; // UDP/IPv6
		SSL_set_mtu(mSsl, static_cast<unsigned int>(mtu));
		PLOG_VERBOSE << "DTLS MTU set to " << mtu << " before incoming records are taken";
	}

	registerIncoming();
	changeState(State::Connecting);

	int ret, err;
	{
		std::lock_guard lock(mSslMutex);
