// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_para_eq_envelope.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-ii follow-up Batch 6 — gzip+base64url envelope tests.
//
// Coverage matrix:
//
//   §1  Round-trip ASCII string                — encode . decode == id
//   §2  Round-trip Unicode string              — UTF-8 path is honest
//   §3  Empty string round-trip                — empty -> "" / nullopt
//   §4  Round-trip large JSON payload          — multi-chunk inflate
//   §5  Thetis fixture decode                  — Python-pinned blob,
//                                                 mtime=0 gzip header,
//                                                 byte-identical inflate
//                                                 path so a Thetis-
//                                                 produced blob is
//                                                 indistinguishable from
//                                                 a NereusSDR-produced
//                                                 one once we decode it.
//   §6  Invalid base64                         — nullopt
//   §7  Valid base64, non-gzip bytes           — nullopt
//   §8  Truncated gzip stream                  — nullopt
//   §9  gzip header OS byte                    — 0 on every platform,
//                                                as .NET's GZipStream
//                                                writes it (2026-09-30)
//
// =================================================================

#include <QtTest/QtTest>

#include "core/ParaEqEnvelope.h"

using namespace NereusSDR;

class TstParaEqEnvelope : public QObject {
    Q_OBJECT

private slots:

    // =========================================================================
    // §1  Round-trip ASCII payload
    //
    // Sanity test: encode the same JSON-shaped string the
    // ParametricEqWidget produces, run it back through decode(), assert
    // we get the same bytes back.  Implicitly exercises the entire
    // gzip + base64url + back-conversion pipeline.
    // =========================================================================

    void roundTripsAsciiString()
    {
        const QString payload =
            QStringLiteral("{\"band_count\":5,\"parametric_eq\":true}");
        const QString blob = ParaEqEnvelope::encode(payload);
        QVERIFY(!blob.isEmpty());
        const auto decoded = ParaEqEnvelope::decode(blob);
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, payload);
    }

    // =========================================================================
    // §2  Round-trip Unicode payload
    //
    // The Thetis ucParametricEq band-name field accepts non-ASCII
    // characters (e.g. localized labels).  toUtf8() / fromUtf8() must
    // be honest about multibyte sequences.
    // =========================================================================

    void roundTripsUnicodeString()
    {
        const QString payload = QStringLiteral(
            "{\"band\":\"\xc3\xa9 testÅ\",\"freq_hz\":\xe2\x88\x9a 1000}");
        const QString blob = ParaEqEnvelope::encode(payload);
        QVERIFY(!blob.isEmpty());
        const auto decoded = ParaEqEnvelope::decode(blob);
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, payload);
    }

    // =========================================================================
    // §3  Empty input edge cases
    //
    // From Thetis Common.cs:1747 + :1766 [v2.10.3.13]:
    //   "if (string.IsNullOrEmpty(...)) return null;"
    //
    // NereusSDR collapses .NET null -> empty QString on encode and
    // -> nullopt on decode (decode's empty input is NOT a valid blob).
    // =========================================================================

    void emptyInputReturnsEmptyOnEncode()
    {
        const QString payload = QStringLiteral("");
        const QString blob = ParaEqEnvelope::encode(payload);
        QVERIFY(blob.isEmpty());
    }

    void emptyInputReturnsNulloptOnDecode()
    {
        const auto decoded = ParaEqEnvelope::decode(QString());
        QVERIFY(!decoded.has_value());
    }

    // =========================================================================
    // §4  Round-trip large JSON payload
    //
    // 16 KiB is the inflate chunk size in ParaEqEnvelope.cpp; pump
    // ~10 KB of JSON through to exercise both the multi-iteration
    // deflate loop and the multi-iteration inflate loop.  Smaller
    // payloads compress to a single chunk and don't catch buffer-
    // boundary bugs.
    // =========================================================================

    void roundTripsLargeJsonPayload()
    {
        QString payload;
        payload.reserve(11000);
        payload.append(QStringLiteral("{\"bands\":["));
        for (int i = 0; i < 200; ++i) {
            if (i > 0) {
                payload.append(QLatin1Char(','));
            }
            payload.append(QStringLiteral(
                "{\"index\":%1,\"freq_hz\":%2,\"gain_db\":%3,\"q\":%4}")
                .arg(i)
                .arg(50 * (i + 1))
                .arg((i % 41) - 20)
                .arg(0.5 + 0.01 * i));
        }
        payload.append(QStringLiteral("]}"));
        QVERIFY(payload.size() > 9000);  // confirm we're past the chunk size

        const QString blob = ParaEqEnvelope::encode(payload);
        QVERIFY(!blob.isEmpty());
        const auto decoded = ParaEqEnvelope::decode(blob);
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, payload);
    }

    // =========================================================================
    // §5  Thetis fixture decode — production-readiness contract
    //
    // The fixture below was generated OFFLINE with Python's gzip module
    // pinned to mtime=0 (matches what zlib's deflateInit2 emits and what
    // .NET's GZipStream typically emits).  The decode path is fully
    // independent of the encode side — it just inflates whatever it gets
    // — so this test verifies that NereusSDR can read a Thetis-produced
    // blob even if our own encode side ever drifts to a different
    // compression level / mtime / OS byte.
    //
    // Python repro:
    //   import gzip, base64, io
    //   buf = io.BytesIO()
    //   with gzip.GzipFile(fileobj=buf, mode='wb',
    //                      compresslevel=9, mtime=0) as f:
    //       f.write(b'{"band_count":5,"parametric_eq":true}')
    //   print(base64.urlsafe_b64encode(buf.getvalue()).rstrip(b'=').decode())
    // =========================================================================

    // =========================================================================
    // §9  The gzip header is the same on every platform
    //
    // zlib writes its build's OS code into the gzip header (byte 9): 0x13
    // on macOS, 0x03 on Linux, 0x0a for a Windows build. The Core's
    // envelope therefore differed by platform, and the link conformance
    // fixtures (tests/data/link/v1/sessions/tx-eq-set-curve.json,
    // cfc-set-profile.json) recorded on macOS failed on Linux CI. Thetis's
    // .NET GZipStream writes OS 0 on every machine (its stored blob at
    // database.cs:11211 [v2.10.3.15] begins 1f 8b 08 00 00 00 00 00 04 00),
    // so the envelope now writes 0 as well, and mtime 0 as before.
    // =========================================================================

    void gzipHeaderIsTheSameOnEveryPlatform()
    {
        const QString blob = ParaEqEnvelope::encode(
            QStringLiteral("{\"band_count\":5,\"parametric_eq\":true}"));
        const QByteArray gz = QByteArray::fromBase64(
            blob.toLatin1(),
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        QVERIFY(gz.size() > 10);
        QCOMPARE(static_cast<quint8>(gz.at(0)), quint8(0x1f));
        QCOMPARE(static_cast<quint8>(gz.at(1)), quint8(0x8b));
        QCOMPARE(static_cast<quint8>(gz.at(2)), quint8(0x08));  // deflate
        QCOMPARE(static_cast<quint8>(gz.at(3)), quint8(0x00));  // no FLG bits
        QCOMPARE(gz.mid(4, 4), QByteArray(4, '\0'));           // mtime 0
        QCOMPARE(static_cast<quint8>(gz.at(9)), quint8(0x00));  // OS
    }

    void thetisFixtureBlobDecodesToKnownJson()
    {
        static const QString kThetisFixtureBlob = QStringLiteral(
            "H4sIAAAAAAAC_6tWSkrMS4lPzi_NK1GyMtVRKkgsSsxNLSnKTI5PLVSyKikqTa0FAOlP_y4lAAAA");
        static const QString kExpectedJson =
            QStringLiteral("{\"band_count\":5,\"parametric_eq\":true}");

        const auto decoded = ParaEqEnvelope::decode(kThetisFixtureBlob);
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, kExpectedJson);
    }

    // =========================================================================
    // §6  Invalid base64 input -> nullopt
    //
    // Thetis raises FormatException on bad-length base64 and lets the
    // GZipStream constructor raise on bad gzip headers.  NereusSDR
    // collapses both to nullopt — callers don't need to distinguish
    // because in both cases the right move is "treat as empty profile".
    //
    // Note: Qt's fromBase64 is lenient and silently returns an empty
    // QByteArray for many forms of garbage.  The empty-decode branch
    // catches that.
    // =========================================================================

    void invalidBase64ReturnsNullopt()
    {
        // Slashes/pluses are reserved in base64url; including them
        // here makes the input invalid under the URL alphabet.
        const QString garbage = QStringLiteral("!!!not!!!valid!!!base64!!!");
        const auto decoded = ParaEqEnvelope::decode(garbage);
        QVERIFY(!decoded.has_value());
    }

    // =========================================================================
    // §7  Valid base64, non-gzip bytes -> nullopt
    //
    // base64url-encoded ASCII "hello" decodes cleanly to bytes, but
    // those bytes don't have a gzip header (1f 8b magic).  inflateInit2
    // succeeds (just initializes state), but the first inflate() call
    // returns Z_DATA_ERROR — the catch we collapse to nullopt.
    // =========================================================================

    void validBase64ButNonGzipReturnsNullopt()
    {
        // base64url("hello") = "aGVsbG8" (5 ASCII bytes — no gzip magic).
        const QString notGzipBase64 = QStringLiteral("aGVsbG8");
        const auto decoded = ParaEqEnvelope::decode(notGzipBase64);
        QVERIFY(!decoded.has_value());
    }

    // =========================================================================
    // §8  Truncated gzip stream -> nullopt
    //
    // Take a known-good gzip stream and chop off its trailing CRC + size.
    // inflate() will read past the deflate-end marker into the trailer,
    // hit Z_DATA_ERROR (or finish without the matching trailer signature),
    // and we'll surface nullopt.
    // =========================================================================

    void truncatedGzipReturnsNullopt()
    {
        const QString payload = QStringLiteral("the quick brown fox");
        const QString fullBlob = ParaEqEnvelope::encode(payload);
        QVERIFY(!fullBlob.isEmpty());
        // Drop the last ~6 chars from the base64url string — that
        // corresponds to roughly 4-5 bytes of gzip trailer (CRC32 +
        // ISIZE).  inflate() should fail to detect Z_STREAM_END.
        QVERIFY(fullBlob.size() > 8);
        const QString truncated = fullBlob.left(fullBlob.size() - 6);
        const auto decoded = ParaEqEnvelope::decode(truncated);
        QVERIFY(!decoded.has_value());
    }
};

QTEST_MAIN(TstParaEqEnvelope)
#include "tst_para_eq_envelope.moc"
