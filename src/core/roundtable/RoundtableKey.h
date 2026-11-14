// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/roundtable/RoundtableKey.h  (Longpath)
// =================================================================
//
// Der Stationsschluessel fuer die Roundtable-Anmeldung: ein P-256-Schluessel,
// sein oeffentlicher Teil als SubjectPublicKeyInfo und die Signatur ueber
// das Anmelde-Transkript. Die Regeln stehen im Roundtable-Protokoll v1,
// Abschnitt 4.1 und 4.3 (Upstream NereusSDR,
// docs/architecture/2026-10-23-roundtable-v1.md @ 9036ffc04):
//
//   - spki: kanonische 91 Byte DER, Punkt unkomprimiert;
//   - sig:  ECDSA P-256 ueber SHA-256, ROH r || s, 64 Byte, nicht DER;
//           ein hohes s ist gueltig.
//
// Upstream nimmt dafuer OpenSSL 3 (src/core/security/StationIdentity.cpp).
// Longpath hat keine OpenSSL-Abhaengigkeit und bekommt auch keine: die
// Rechnung macht eine Teilmenge von Mbed TLS 3.6.7 (third_party/mbedtls),
// die Signatur deterministisch nach RFC 6979. Der Zufall fuer den neuen
// Schluessel und fuer die Seitenkanal-Blendung kommt aus
// QRandomGenerator::system(), dem Zufall des Betriebssystems.
//
// Die Schluesseldatei ist PKCS#8 als PEM ("BEGIN PRIVATE KEY"), dieselbe
// Form, die OpenSSL und Pythons cryptography schreiben und lesen, und sie
// gehoert nur dem Benutzer (0600) — wie Upstreams station-identity.pem.
//
// Longpath-eigen; die Dateiregeln (0600, vor dem Umbenennen gesetzt) folgen
// Upstreams StationIdentity::loadOrCreateKeyFile.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QByteArray>
#include <QString>

#include <memory>

namespace Longpath {

class RoundtableKey {
public:
    RoundtableKey();
    ~RoundtableKey();
    RoundtableKey(RoundtableKey&&) noexcept;
    RoundtableKey& operator=(RoundtableKey&&) noexcept;
    RoundtableKey(const RoundtableKey&) = delete;
    RoundtableKey& operator=(const RoundtableKey&) = delete;

    /// Ein neuer Schluessel aus dem Zufall des Betriebssystems.
    static RoundtableKey generate(QString* error = nullptr);
    /// Aus dem geheimen Skalar (32 Byte, big-endian). Fuer die Pruefdaten.
    static RoundtableKey fromPrivateScalar(const QByteArray& d, QString* error = nullptr);
    /// Aus einer PKCS#8-PEM ("BEGIN PRIVATE KEY") mit einem P-256-Schluessel.
    static RoundtableKey fromPkcs8Pem(const QByteArray& pem, QString* error = nullptr);
    /// Die Datei lesen; fehlt sie, einen Schluessel anlegen und mit 0600
    /// schreiben. Ist sie fuer andere lesbar, wird sie wieder 0600 gemacht.
    static RoundtableKey loadOrCreate(const QString& path, QString* error = nullptr);

    bool isValid() const;

    /// Die 91 Byte SubjectPublicKeyInfo (4.1).
    QByteArray spkiDer() const;
    /// Die 122 Zeichen, die `hello` als `spki` schickt.
    QString spkiBase64Url() const;
    /// Die Roundtable-Id (4.1), 26 Zeichen aus a-z und 2-7.
    QString roundtableId() const;
    /// PKCS#8 als PEM, so wie loadOrCreate() es schreibt.
    QByteArray pkcs8Pem() const;

    /// ECDSA P-256 ueber SHA-256(message), roh r || s (64 Byte).
    /// Leer, wenn der Schluessel ungueltig ist.
    QByteArray sign(const QByteArray& message) const;

    /// Prueft eine rohe Signatur (64 Byte) gegen eine SPKI (91 Byte).
    static bool verify(const QByteArray& spkiDer, const QByteArray& message,
                       const QByteArray& signature);
    /// Eine SPKI ist nur in der kanonischen Form gueltig (4.1): 91 Byte,
    /// P-256, Punkt unkomprimiert und auf der Kurve.
    static bool isCanonicalSpki(const QByteArray& spkiDer);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Longpath
