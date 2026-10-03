// =================================================================
// tests/tst_certificate_store.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Remote Daemon R2,
// Task 17: TLS certificate provisioning for nereusd's wss:// listener.
//
// Design source: docs/architecture/2026-07-28-remote-daemon-architecture-
// design.md §10.5 "Encryption, and which phase owns it" -- "the daemon
// generates a self-signed certificate on first run and the client pins
// its fingerprint, displayed at pairing time alongside the token (§7.1)".
//
// Every slot below constructs its own CertificateStore against a fresh
// QTemporaryDir, never AppSettings::kDaemonProfileName's real directory --
// a test run must not leave (or read back) a TLS identity in the
// developer's actual daemon profile.
// =================================================================

#include <QtTest>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/security/CertificateStore.h"
#include "core/security/TokenStore.h"
#include "fakes/UpgradedCoreToken.h"

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace NereusSDR;

namespace {

// 32 colon-separated uppercase hex byte pairs -- a SHA-256 digest
// formatted the conventional X.509-fingerprint way.
const QRegularExpression kFingerprintPattern(
    QStringLiteral("^([0-9A-F]{2}:){31}[0-9A-F]{2}$"));

#ifdef Q_OS_UNIX
// Fix round 3 minor: restores a path's permissions on scope exit
// (destructor, so this runs on an early return too), rather than a bare
// QFile::setPermissions() call placed after the assertions it is meant
// to clean up after. Two slots below deliberately leave a path in a
// permission state QTemporaryDir's own destructor cannot remove
// (unreadable file, unwritable directory); a prior revision restored
// permissions only on the success path, so a QVERIFY failing between the
// chmod and the restore left an unremovable QTemporaryDir behind.
struct PermissionRestorer {
    QString path;
    QFileDevice::Permissions restoreTo;
    ~PermissionRestorer() { QFile::setPermissions(path, restoreTo); }
};
#endif

} // namespace

class TstCertificateStore : public QObject {
    Q_OBJECT
private slots:

    // First run: no cert/key on disk yet (the directory itself does not
    // even exist -- exercises the mkpath path), so the constructor must
    // generate both, write them to <dir>/, and hand back something
    // usable. "Assert both parse back through QSslCertificate and
    // QSslKey" (task-17-brief.md Step 1) is checked twice: once via the
    // store's own accessors, and once by independently re-reading the
    // on-disk PEM bytes and re-parsing them in this test -- proving the
    // on-disk artifact itself round-trips, not just whatever the store
    // happens to be holding in memory.
    void generatesOnFirstRun()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.path() + QStringLiteral("/not_yet_created");

        CertificateStore store(dir);
        QVERIFY2(store.isValid(), qPrintable(store.lastError()));
        QVERIFY(store.lastError().isEmpty());

        QVERIFY(QFileInfo::exists(store.certificatePath()));
        QVERIFY(QFileInfo::exists(store.privateKeyPath()));

        const QSslCertificate cert = store.certificate();
        QVERIFY(!cert.isNull());
        QVERIFY(cert.isSelfSigned());

        const QSslKey key = store.privateKey();
        QVERIFY(!key.isNull());
        QCOMPARE(key.type(), QSsl::PrivateKey);
        QCOMPARE(key.algorithm(), CertificateStore::kKeyAlgorithm);

        QVERIFY(kFingerprintPattern.match(store.fingerprintSha256()).hasMatch());

        // Independent re-parse of the on-disk bytes, not the store's
        // cached objects.
        QFile certFile(store.certificatePath());
        QVERIFY(certFile.open(QIODevice::ReadOnly));
        const QSslCertificate reparsedCert(certFile.readAll(), QSsl::Pem);
        QVERIFY(!reparsedCert.isNull());
        QCOMPARE(reparsedCert.digest(QCryptographicHash::Sha256),
                 cert.digest(QCryptographicHash::Sha256));

        QFile keyFile(store.privateKeyPath());
        QVERIFY(keyFile.open(QIODevice::ReadOnly));
        const QSslKey reparsedKey(keyFile.readAll(), CertificateStore::kKeyAlgorithm,
                                   QSsl::Pem, QSsl::PrivateKey);
        QVERIFY(!reparsedKey.isNull());
    }

    // The assertion most likely to pass vacuously (task-17-controller-
    // notes.md): a freshly generated certificate also parses fine, so
    // merely checking the second store is "valid" proves nothing about
    // reuse. What actually proves it: RSA key generation is randomised,
    // so two INDEPENDENT generations can never produce the same
    // fingerprint or the same PEM bytes by chance. Comparing both across
    // two CertificateStore constructions against the same directory is
    // therefore real evidence the second construction loaded the first
    // construction's files rather than overwriting them.
    void reusesRatherThanRegeneratesOnSecondRun()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        CertificateStore first(tmp.path());
        QVERIFY2(first.isValid(), qPrintable(first.lastError()));
        const QString fingerprint1 = first.fingerprintSha256();
        QVERIFY(!fingerprint1.isEmpty());

        QFile certFile1(first.certificatePath());
        QVERIFY(certFile1.open(QIODevice::ReadOnly));
        const QByteArray certBytes1 = certFile1.readAll();
        QFile keyFile1(first.privateKeyPath());
        QVERIFY(keyFile1.open(QIODevice::ReadOnly));
        const QByteArray keyBytes1 = keyFile1.readAll();

        CertificateStore second(tmp.path());
        QVERIFY2(second.isValid(), qPrintable(second.lastError()));
        const QString fingerprint2 = second.fingerprintSha256();

        QCOMPARE(fingerprint2, fingerprint1);

        QFile certFile2(second.certificatePath());
        QVERIFY(certFile2.open(QIODevice::ReadOnly));
        QCOMPARE(certFile2.readAll(), certBytes1);
        QFile keyFile2(second.privateKeyPath());
        QVERIFY(keyFile2.open(QIODevice::ReadOnly));
        QCOMPARE(keyFile2.readAll(), keyBytes1);
    }

    // Corrupt (not merely missing) files on disk must not wedge the
    // daemon permanently: treat an unparseable pair the same as a first
    // run rather than leaving isValid() false forever.
    void regeneratesWhenStoredFilesAreCorrupt()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        CertificateStore first(tmp.path());
        QVERIFY2(first.isValid(), qPrintable(first.lastError()));
        const QString certPath = first.certificatePath();
        const QString keyPath  = first.privateKeyPath();
        // Fix round 1, review minor 11: capture the original fingerprint
        // so "regenerates" is asserted positively below (a genuinely new
        // identity), not just inferred from the recovered fingerprint's
        // format being well-formed.
        const QString originalFingerprint = first.fingerprintSha256();

        {
            QFile certFile(certPath);
            QVERIFY(certFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
            certFile.write("not a certificate\n");
        }
        {
            QFile keyFile(keyPath);
            QVERIFY(keyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
            keyFile.write("not a key\n");
        }

        CertificateStore recovered(tmp.path());
        QVERIFY2(recovered.isValid(), qPrintable(recovered.lastError()));
        QVERIFY(!recovered.certificate().isNull());
        QVERIFY(!recovered.privateKey().isNull());
        QVERIFY(kFingerprintPattern.match(recovered.fingerprintSha256()).hasMatch());
        QVERIFY(recovered.fingerprintSha256() != originalFingerprint);
    }

    // Fix round 1, Important 2: a certificate and key that are each
    // individually well-formed PEM, but do not belong to the same key
    // pair, must be rejected rather than silently accepted as valid.
    // This is exactly what a write that fails between the certificate
    // write and the key write in generateAndStore() (ENOSPC on an SD
    // card being the realistic case) leaves on disk: a NEW certificate
    // beside the OLD key, or vice versa.
    //
    // The class has no way to report "rejected, and here is why" other
    // than by falling through to regeneration -- the same self-healing
    // path regeneratesWhenStoredFilesAreCorrupt() above already pins for
    // unparseable files. So the observable proof from outside the class
    // is: reconstructing over a mismatched pair produces a store that is
    // valid again (regeneration succeeded) with a certificate that is
    // NOT the foreign one that was sitting on disk -- proving the
    // mismatched pair was not the thing served.
    //
    // Fails before the X509_check_private_key() fix (the foreign
    // certificate parses individually, so unfixed loadExisting() accepts
    // it and fingerprintSha256() equals foreignFingerprint); passes after.
    void rejectsMismatchedCertificateAndKeyPair()
    {
        QTemporaryDir tmpA;
        QVERIFY(tmpA.isValid());
        CertificateStore storeA(tmpA.path());
        QVERIFY2(storeA.isValid(), qPrintable(storeA.lastError()));

        QTemporaryDir tmpB;
        QVERIFY(tmpB.isValid());
        CertificateStore storeB(tmpB.path());
        QVERIFY2(storeB.isValid(), qPrintable(storeB.lastError()));
        const QString foreignFingerprint = storeB.fingerprintSha256();
        QVERIFY(!foreignFingerprint.isEmpty());
        QVERIFY(foreignFingerprint != storeA.fingerprintSha256());

        QFile foreignCertFile(storeB.certificatePath());
        QVERIFY(foreignCertFile.open(QIODevice::ReadOnly));
        const QByteArray foreignCertPem = foreignCertFile.readAll();

        // storeA's key is left untouched; only its certificate is
        // replaced with storeB's, unrelated, certificate -- a mismatched
        // pair, each half individually valid PEM.
        {
            QFile certA(storeA.certificatePath());
            QVERIFY(certA.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(certA.write(foreignCertPem),
                     static_cast<qint64>(foreignCertPem.size()));
        }

        CertificateStore reloaded(tmpA.path());
        QVERIFY2(reloaded.isValid(), qPrintable(reloaded.lastError()));
        QVERIFY(reloaded.fingerprintSha256() != foreignFingerprint);
    }

#ifdef Q_OS_UNIX
    // Fix round 1, Important 3: an EXISTING private key this process
    // cannot read (mode 0000, simulating a permission mismatch between
    // the user who first provisioned the daemon and the user it is
    // later run as -- systemd's User=, for example) must fail loudly,
    // not be silently treated the same as "no identity yet" and
    // regenerated over. Regenerating here would destroy a pinned station
    // identity every already-paired client depends on, with no error and
    // no log line -- see task-17-report.md's fix-round section for the
    // full scenario.
    //
    // Root bypasses UNIX permission checks entirely, which would make
    // this pass vacuously (the "unreadable" file would still open fine),
    // so this QSKIPs under root rather than asserting something it
    // cannot actually test.
    //
    // Fails before the exists()-vs-open() fix, but not for the reason an
    // earlier revision of this comment claimed. Fix round 3 correction
    // (reviewer-verified): the unfixed code does not fail to overwrite
    // the mode-0000 key, it succeeds. QSaveFile::commit() never opens the
    // EXISTING file at all -- it writes a new temp file elsewhere and
    // atomically rename()s it over the old name, and rename() is a
    // DIRECTORY-entry operation gated on the directory's write
    // permission, not the target file's own permission bits. The
    // unfixed `generateAndStore()` fallback therefore overwrites BOTH
    // files successfully (the directory is still fully writable in this
    // test; only the key FILE's own mode is 0000), silently replacing
    // the pinned identity with a fresh, unrelated one. Fixed
    // loadExisting() reports the I/O failure as a hard error instead of
    // falling through to that fallback at all, so no write of either
    // file is ever attempted.
    void unreadableExistingKeyFailsRatherThanRegenerates()
    {
        if (geteuid() == 0) {
            QSKIP("Running as root, which bypasses UNIX permission checks; "
                  "this test cannot exercise an unreadable-but-existing file.");
        }

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        CertificateStore first(tmp.path());
        QVERIFY2(first.isValid(), qPrintable(first.lastError()));
        const QString fingerprint1 = first.fingerprintSha256();
        const QString keyPath = first.privateKeyPath();

        QVERIFY(QFile::setPermissions(keyPath, QFileDevice::Permissions()));
        // RAII (fix round 3 minor): restores permissions on scope exit,
        // including an early return from a QVERIFY below, so a failing
        // assertion here never leaves QTemporaryDir unable to clean up
        // the still-mode-0000 key file.
        const PermissionRestorer restorer{
            keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner};

        CertificateStore second(tmp.path());
        QVERIFY(!second.isValid());
        QVERIFY(!second.lastError().isEmpty());

        // Confirm the ORIGINAL identity survived untouched (the whole
        // point of failing loudly instead of regenerating), now that
        // permissions are restored.
        QVERIFY(QFile::setPermissions(
            keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        CertificateStore third(tmp.path());
        QVERIFY2(third.isValid(), qPrintable(third.lastError()));
        QCOMPARE(third.fingerprintSha256(), fingerprint1);
    }

    // Fix round 1, Important 4: a directory that exists but cannot be
    // written into (first run, no cert/key yet, so generateAndStore()
    // must create the files) must fail cleanly with isValid() == false
    // and a non-empty lastError(), not crash or silently succeed.
    //
    // Root bypasses this too (ignores the missing write bit), hence the
    // same QSKIP guard as the test above.
    void constructingAgainstAnUnwritableDirectoryFails()
    {
        if (geteuid() == 0) {
            QSKIP("Running as root, which bypasses UNIX permission checks; "
                  "this test cannot exercise an unwritable directory.");
        }

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(QFile::setPermissions(
            tmp.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        // RAII (fix round 3 minor): restores the write bit on scope exit,
        // including an early return from a QVERIFY below, so a failing
        // assertion here never leaves QTemporaryDir unable to remove its
        // own still-unwritable directory. A prior revision restored this
        // only after both assertions below, on the success path alone.
        const PermissionRestorer restorer{
            tmp.path(),
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner};

        CertificateStore store(tmp.path());
        QVERIFY(!store.isValid());
        QVERIFY(!store.lastError().isEmpty());
    }
#endif

    // Fix round 1, Important 4: a genuinely MISSING file (as opposed to
    // Important 3's unreadable-but-present file) must still regenerate
    // cleanly. This is the negative control proving the Important 3 fix
    // is scoped to "exists but cannot be opened" and does not also
    // start rejecting the legitimately-missing case an operator hits on
    // every real first run.
    void deletingOneFileAfterFirstRunStillRegenerates()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        CertificateStore first(tmp.path());
        QVERIFY2(first.isValid(), qPrintable(first.lastError()));
        const QString originalFingerprint = first.fingerprintSha256();
        const QString keyPath = first.privateKeyPath();

        // Delete only the key; the certificate is left in place. This is
        // the "missing" half of the exists()-vs-open() distinction
        // Important 3 draws: loadExisting()'s own
        // `!certFile.exists() || !keyFile.exists()` check must still
        // catch this (it is unchanged by the fix) and fall through to
        // regeneration, which replaces BOTH files -- so the recovered
        // fingerprint legitimately differs from the original.
        QVERIFY(QFile::remove(keyPath));

        CertificateStore recovered(tmp.path());
        QVERIFY2(recovered.isValid(), qPrintable(recovered.lastError()));
        QVERIFY(kFingerprintPattern.match(recovered.fingerprintSha256()).hasMatch());
        QVERIFY(recovered.fingerprintSha256() != originalFingerprint);
    }

    // task-17-brief.md Step 1: guard the file-permission assertion with
    // #ifdef Q_OS_UNIX and QSKIP on Windows, where QFile::permissions()
    // mirrors owner bits into group and other, making a 0600-style
    // assertion pass vacuously or fail spuriously depending on which way
    // it is written.
    void privateKeyFileIsOwnerOnlyOnUnix()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        CertificateStore store(tmp.path());
        QVERIFY2(store.isValid(), qPrintable(store.lastError()));

#ifdef Q_OS_UNIX
        const QFileDevice::Permissions perms =
            QFile(store.privateKeyPath()).permissions();
        QVERIFY(perms.testFlag(QFileDevice::ReadOwner));
        QVERIFY(perms.testFlag(QFileDevice::WriteOwner));
        const QFileDevice::Permissions forbidden =
            QFileDevice::ReadGroup  | QFileDevice::WriteGroup  | QFileDevice::ExeGroup |
            QFileDevice::ReadOther  | QFileDevice::WriteOther  | QFileDevice::ExeOther;
        QCOMPARE(perms & forbidden, QFileDevice::Permissions());
#else
        QSKIP("QFile::permissions() mirrors owner bits into group/other on "
              "Windows (task-17-brief.md Step 1); the on-disk ACL is set "
              "best-effort by CertificateStore but not meaningfully "
              "assertable here.");
#endif
    }

    // LINK minor 6: a private key loosened on disk after it was written is
    // made owner-only again when the store loads it.
    void aLoosenedKeyIsOwnerOnlyAgainOnLoad()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString keyPath;
        QString fingerprint;
        {
            CertificateStore first(tmp.path());
            QVERIFY2(first.isValid(), qPrintable(first.lastError()));
            keyPath = first.privateKeyPath();
            fingerprint = first.fingerprintSha256();
        }
        QVERIFY(QFile::setPermissions(keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                   | QFileDevice::ReadGroup
                                                   | QFileDevice::ReadOther));
        CertificateStore second(tmp.path());
        QVERIFY2(second.isValid(), qPrintable(second.lastError()));
        QCOMPARE(second.fingerprintSha256(), fingerprint);  // loaded, not regenerated
        const QFileDevice::Permissions forbidden =
            QFileDevice::ReadGroup  | QFileDevice::WriteGroup  | QFileDevice::ExeGroup |
            QFileDevice::ReadOther  | QFileDevice::WriteOther  | QFileDevice::ExeOther;
        QCOMPARE(QFile(keyPath).permissions() & forbidden, QFileDevice::Permissions());

        // The token file, the same way.
        const QString tokenDir =
            NereusSDR::Test::seedUpgradedCoreToken(tmp.filePath(QStringLiteral("token")));
        const QString tokenPath = TokenStore(tokenDir).tokenPath();
        QVERIFY(QFile::setPermissions(tokenPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                     | QFileDevice::ReadOther));
        TokenStore token(tokenDir);
        QVERIFY(token.isActive());
        QCOMPARE(QFile(tokenPath).permissions() & forbidden, QFileDevice::Permissions());
#else
        QSKIP("QFile::permissions() mirrors owner bits into group/other on Windows.");
#endif
    }

    // task-17-brief.md Step 4: handle QSslSocket::supportsSsl() == false
    // with a message naming the cause, rather than a generic "TLS
    // unavailable". This machine has a working backend either way, so
    // the meaningful assertion is the implication itself: empty
    // diagnostic exactly when Qt reports a working backend, non-empty
    // exactly when it does not -- true on any CI runner regardless of
    // which side of that it lands on.
    //
    // Fix round 1, review minor 11: the trailing QVERIFY(!diagnostic.
    // isEmpty()) a prior revision had here was redundant -- the QCOMPARE
    // above already establishes diagnostic.isEmpty() == supportsSsl(),
    // so "supportsSsl() is false" already implies "diagnostic is
    // non-empty" with nothing left to separately check.
    void tlsBackendDiagnosticMatchesSupportsSsl()
    {
        const QString diagnostic = CertificateStore::tlsBackendDiagnostic();
        QCOMPARE(diagnostic.isEmpty(), QSslSocket::supportsSsl());
    }

    // task-17-controller-notes.md: "resolve it through
    // AppSettings::resolveConfigDir rather than rebuilding the path by
    // hand". Pinned directly against the sanctioned resolver so a future
    // edit that starts hand-building an equivalent-looking path (and
    // silently drifts from it) fails here instead of only showing up as
    // a runtime surprise on a real daemon profile.
    void defaultDirectoryUsesAppSettingsResolver()
    {
        QCOMPARE(CertificateStore::defaultDirectory(),
                 AppSettings::resolveConfigDir(
                     QString::fromLatin1(AppSettings::kDaemonProfileName)));
    }

    // Security material has to follow --profile, or --profile does not
    // isolate what it claims to isolate.
    //
    // Both stores used to hardcode kDaemonProfileName, and
    // DaemonApp::startStationServer passes no securityDirectory, so
    // `nereusd --profile alpha` and `nereusd --profile beta` isolated
    // their settings and their logs and then SHARED one certificate and
    // one token. Two simultaneous first runs each minted a token and raced
    // the rename, and the loser went on to accept a token that was not the
    // one on disk. --profile exists to isolate instances on one
    // workstation, so that isolation was incomplete in exactly the place
    // it matters most.
    //
    // Asserted on BOTH stores in one slot: it is one invariant, and
    // splitting it across two files is how half of it later drifts.
    void securityDirectoryFollowsTheProfileOverride()
    {
        // setProfileOverride is process-global, so it is restored on every
        // exit path -- each QCOMPARE below is a bare return.
        const QString saved = AppSettings::profileOverride();
        const auto restore =
            qScopeGuard([&saved]() { AppSettings::setProfileOverride(saved); });

        AppSettings::setProfileOverride(QStringLiteral("alpha"));
        const QString alphaCert = CertificateStore::defaultDirectory();
        const QString alphaToken = TokenStore::defaultDirectory();
        QCOMPARE(alphaCert, AppSettings::resolveConfigDir(QStringLiteral("alpha")));
        QCOMPARE(alphaToken, alphaCert);

        AppSettings::setProfileOverride(QStringLiteral("beta"));
        const QString betaCert = CertificateStore::defaultDirectory();
        const QString betaToken = TokenStore::defaultDirectory();
        QCOMPARE(betaCert, AppSettings::resolveConfigDir(QStringLiteral("beta")));
        QCOMPARE(betaToken, betaCert);

        QVERIFY2(alphaCert != betaCert,
                 "two --profile instances resolved to the same TLS identity "
                 "directory, so they share one certificate and one token");
        QVERIFY2(alphaToken != betaToken,
                 "two --profile instances resolved to the same token directory");

        // And they both track the SETTINGS directory, which is the whole
        // claim: security material sits beside the store its instance uses.
        QCOMPARE(alphaCert,
                 QFileInfo(AppSettings::resolveSettingsPath(QStringLiteral("alpha")))
                     .absolutePath());

        // No override at all (a GUI, or this test binary before the lines
        // above ran) still resolves to the RESERVED daemon profile, never
        // to the user's shared config directory. See
        // CertificateStore::defaultDirectory() for why the fallback is
        // deliberate rather than defensive.
        AppSettings::setProfileOverride(QString());
        QCOMPARE(CertificateStore::defaultDirectory(),
                 AppSettings::resolveConfigDir(
                     QString::fromLatin1(AppSettings::kDaemonProfileName)));
        QCOMPARE(TokenStore::defaultDirectory(), CertificateStore::defaultDirectory());
    }
};

QTEST_MAIN(TstCertificateStore)
#include "tst_certificate_store.moc"
