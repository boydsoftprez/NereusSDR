// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_surface_manifest_regen.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 1 (R-IOS-01): writes LinkSurface::capture() as
// surface.json. Into the directory NEREUS_LINK_REGEN_OUT names when it is
// set, and otherwise into a temporary directory, so ctest runs it without
// touching the tree (the precedent is tst_tci_init_burst_regen). Not a
// regression check; tst_link_surface_manifest is.
//
//   cmake --build build --target tst_link_surface_manifest_regen
//   NEREUS_LINK_REGEN_OUT=tests/data/link/v1 QT_QPA_PLATFORM=offscreen \
//       ./build/tests/tst_link_surface_manifest_regen
//
// Read the resulting diff whole before committing it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01): link
//                                    surface regenerator. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "core/AppSettings.h"

#include "LinkSurface.h"

using namespace NereusSDR;
using NereusSDR::Test::LinkSurface;

class TstLinkSurfaceManifestRegen : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void writeSurface();
};

void TstLinkSurfaceManifestRegen::initTestCase()
{
    const QString profile =
        QStringLiteral("link-surface-regen-%1").arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
    AppSettings::instance().clear();
}

void TstLinkSurfaceManifestRegen::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstLinkSurfaceManifestRegen::writeSurface()
{
    const QJsonObject surface = LinkSurface::capture();
    QCOMPARE(surface.size(), LinkSurface::sectionNames().size());

    QTemporaryDir scratch;
    const QByteArray requested = qgetenv("NEREUS_LINK_REGEN_OUT");
    const QString directory =
        requested.isEmpty() ? scratch.path() : QString::fromLocal8Bit(requested);
    QVERIFY2(!directory.isEmpty(), "no output directory");
    QVERIFY(QDir().mkpath(directory));

    QFile file(QDir(directory).filePath(QStringLiteral("surface.json")));
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.fileName()));
    const QByteArray json = QJsonDocument(surface).toJson(QJsonDocument::Indented);
    QCOMPARE(file.write(json), static_cast<qint64>(json.size()));
    file.close();
    if (!requested.isEmpty()) {
        qInfo().noquote() << "wrote" << file.fileName();
    }
}

QTEST_MAIN(TstLinkSurfaceManifestRegen)
#include "tst_link_surface_manifest_regen.moc"
