// R1 Task 4 guard test. src/core and src/models must never #include
// src/gui: that is the invariant the headless nereusd daemon depends on to
// link NereusCore without a widget toolkit. This is a guard test, not a
// unit test -- it asserts a property of the whole tree, not one class's
// behavior, and it is meant to fail loudly the moment anyone reintroduces
// a "gui/" include into either directory.
//
// Fix round 1 findings (reviewer, see task-4-report.md):
//   Finding 1: a wrong NEREUS_SOURCE_DIR scans 0 files, finds 0 offenders,
//     and PASSES -- a silently-green boundary guard for the load-bearing
//     invariant of the whole R1 epic. `scanned` plus the QVERIFY2 floor
//     below turns "found nothing" into a hard failure unless the scan
//     actually walked a real tree (320 files as of this fix: 287 core +
//     33 models).
//   Finding 3: QString::contains(literal) only caught one exact spelling.
//     `${CMAKE_SOURCE_DIR}/src` is a PUBLIC include dir, so `#include
//     <gui/...>`, `#include "../gui/..."`, extra whitespace after `#` or
//     `include`, all compile and all slipped past the literal match. The
//     regex below is the brief's own guard test verbatim except for this
//     one line, which is why it is being fixed here rather than treated
//     as a pre-existing bug outside this task's file.
//
// no-port-check: NereusSDR-original test infrastructure.
#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

class TstCoreHasNoGuiIncludes : public QObject {
    Q_OBJECT
private slots:
    // src/core and src/models must never include src/gui.  This is the
    // invariant that lets nereusd link NereusCore without a widget toolkit.
    void coreNeverIncludesGui()
    {
        // Matches "#include" in any of the forms the include path search
        // treats identically: <gui/...> or "gui/...", any amount of
        // whitespace around "#" and after "include", and any number of
        // leading "../" segments before "gui/".
        static const QRegularExpression kGuiInclude(
            QStringLiteral("^\\s*#\\s*include\\s*[<\"](\\.\\./)*gui/"));

        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QStringList offenders;
        int scanned = 0;
        for (const QString& sub : {QStringLiteral("/src/core"), QStringLiteral("/src/models")}) {
            QDirIterator it(root + sub, {"*.h", "*.cpp", "*.mm"},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
                ++scanned;
                QTextStream ts(&f);
                int line = 0;
                while (!ts.atEnd()) {
                    ++line;
                    const QString l = ts.readLine();
                    if (kGuiInclude.match(l).hasMatch()) {
                        offenders << QStringLiteral("%1:%2").arg(path).arg(line);
                    }
                }
            }
        }
        if (!offenders.isEmpty()) {
            qWarning() << "core/models include gui:" << offenders;
        }
        // A wrong NEREUS_SOURCE_DIR (or a src/core|src/models that stops
        // existing) scans zero files and would otherwise report a false
        // "zero offenders" pass. Fail loudly instead of passing vacuously.
        QVERIFY2(scanned > 100,
                 qPrintable(QString("only %1 files scanned, NEREUS_SOURCE_DIR is probably wrong")
                                .arg(scanned)));
        QCOMPARE(offenders.size(), 0);
    }
};

QTEST_MAIN(TstCoreHasNoGuiIncludes)
#include "tst_core_has_no_gui_includes.moc"
