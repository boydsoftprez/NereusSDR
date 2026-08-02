// R1 Task 4 guard test. src/core and src/models must never #include
// src/gui: that is the invariant the headless nereusd daemon depends on to
// link NereusCore without a widget toolkit. This is a guard test, not a
// unit test -- it asserts a property of the whole tree, not one class's
// behavior, and it is meant to fail loudly the moment anyone reintroduces
// a "gui/" include into either directory.
//
// no-port-check: NereusSDR-original test infrastructure.
#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>

class TstCoreHasNoGuiIncludes : public QObject {
    Q_OBJECT
private slots:
    // src/core and src/models must never include src/gui.  This is the
    // invariant that lets nereusd link NereusCore without a widget toolkit.
    void coreNeverIncludesGui()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QStringList offenders;
        for (const QString& sub : {QStringLiteral("/src/core"), QStringLiteral("/src/models")}) {
            QDirIterator it(root + sub, {"*.h", "*.cpp", "*.mm"},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
                QTextStream ts(&f);
                int line = 0;
                while (!ts.atEnd()) {
                    ++line;
                    const QString l = ts.readLine();
                    if (l.contains(QStringLiteral("#include \"gui/"))) {
                        offenders << QStringLiteral("%1:%2").arg(path).arg(line);
                    }
                }
            }
        }
        if (!offenders.isEmpty()) {
            qWarning() << "core/models include gui:" << offenders;
        }
        QCOMPARE(offenders.size(), 0);
    }
};

QTEST_MAIN(TstCoreHasNoGuiIncludes)
#include "tst_core_has_no_gui_includes.moc"
