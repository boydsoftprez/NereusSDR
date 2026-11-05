// no-port-check: NereusSDR-original.
// =================================================================
// tests/TestSharedTlsKey.cpp  (NereusSDR)
// =================================================================
//
// One RSA key for every TLS certificate the tests make, in one file per
// build directory (NEREUS_TEST_TLS_KEY_FILE), made by the first test that
// needs it and never stored in the tree.
//
// A StationServer makes its TLS certificate on first start, and the 3072-bit
// RSA key is by far the costliest step: a test paid it once per Core. 54
// tests failed in one Linux CI run (41764912150): among the 49 that timed
// out, tst_remote_window_transmit printed 19 new identities in 19 functions
// and tst_setup_description_service 25 in 48. withSharedTlsIdentity()
// (fakes/UpgradedCoreToken.h) only helped the tests that asked for it.
//
// Only the key is shared. Each CertificateStore still makes its own
// certificate, with its own random serial, so two Cores in a test present
// different certificates and pins still tell them apart. A test about the
// key itself opts out with OWN_TLS_KEYS in nereus_add_test().
//
// The file is replaced whole (QSaveFile), so a test never reads half of one;
// two tests that start together may each make a key, and the last one
// written is the one later tests read. A missing file is the normal first
// run. An unusable one is replaced, and each process that finds one, or cannot
// save the key, warns once, since every start would otherwise pay the old
// generation time with nothing to say why.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/security/CertificateStore.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>
#include <QString>
#include <QTemporaryDir>

namespace {

// True while this thread makes the key below, whose CertificateStore comes
// back to sharedKeyPem() for its key and must get the real generator.
thread_local bool t_makingKey = false;

QByteArray makeKeyPem()
{
    t_makingKey = true;
    QByteArray pem;
    {
        QTemporaryDir scratch;
        if (scratch.isValid()) {
            const NereusSDR::CertificateStore store(scratch.path());
            QFile key(store.privateKeyPath());
            if (store.isValid() && key.open(QIODevice::ReadOnly)) {
                pem = key.readAll();
            }
        }
    }
    t_makingKey = false;
    return pem;
}

QByteArray loadOrMakeKeyPem()
{
    const QString path = QStringLiteral(NEREUS_TEST_TLS_KEY_FILE);
    if (QFile::exists(path)) {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly)) {
            const QByteArray pem = existing.readAll();
            if (pem.contains("PRIVATE KEY")) {
                return pem;
            }
        }
        qWarning("TestSharedTlsKey: %s holds no usable key; replacing it",
                 qPrintable(path));
    }
    const QByteArray pem = makeKeyPem();
    if (pem.isEmpty()) {
        qWarning("TestSharedTlsKey: could not make a key; each Core makes its own");
        return pem;
    }
    QSaveFile out(path);
    const bool saved = out.open(QIODevice::WriteOnly)
        && out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        && out.write(pem) == pem.size() && out.commit();
    if (!saved) {
        qWarning("TestSharedTlsKey: could not save %s: %s; the next test makes its own key",
                 qPrintable(path), qPrintable(out.errorString()));
    }
    return pem;
}

QByteArray sharedKeyPem()
{
    if (t_makingKey) {
        return {};
    }
    static const QByteArray pem = loadOrMakeKeyPem();
    return pem;
}

struct SharedTlsKeyInit {
    SharedTlsKeyInit() { NereusSDR::CertificateStore::setKeySourceForTest(&sharedKeyPem); }
};

SharedTlsKeyInit g_sharedTlsKeyInit;

} // namespace
