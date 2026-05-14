// tests/tst_tci_matrix_runner.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port.
//
// TCI verification matrix runner.
//
// Loads tests/data/tci/matrix.csv — one row per Thetis behavior — and
// feeds each command column into TciProtocol::handleCommand(). Asserts
// that the synchronous response and the drained notification queue match
// the expected_response and expected_notifications columns.
//
// CSV format (set by Phase 1 Task 1.1):
//   command,input,expected_response,expected_notifications,thetis_cite,notes
//
// CSV escape convention (Phase 6+):
//   Commas inside a cell value are escaped as \, (backslash-comma). The
//   smartSplit() helper below unescapes them so cells like
//   vfo:0\,0\,14250000; are parsed correctly. Rows without \, parse
//   identically to plain split(',') — backslash-free rows are unchanged.
//
// SETUP: prefix convention:
//   The "input" cell may contain one or more SETUP: directives followed
//   by the actual command, all separated by ";;". Example:
//     SETUP:TciVfoAHz=14200000;;vfo:0,0,14200000;
//   Each SETUP:Key=Value segment is applied to AppSettings before the
//   command is dispatched. Non-SETUP early segments are dispatched as
//   commands (notifications drained after each), enabling set;;query
//   chain patterns. The last segment is the command whose response
//   and notifications are asserted on.
//
// Actual data rows are added per phase (Phase 5 onward). Until then the
// CSV is header-only and this test passes vacuously.

#include <QtTest/QtTest>
#include <QFile>
#include <QTextStream>

#include "core/TciProtocol.h"
#include "core/AppSettings.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciMatrixRunner : public QObject {
    Q_OBJECT

    struct Row {
        QString command;
        QString input;
        QString expectedResponse;
        QString expectedNotifs;
        QString cite;
        QString notes;
    };

    // Phase 6: CSV cell splitter that handles \, escaped commas.
    // A backslash followed by a comma produces a literal comma in the cell.
    // A plain comma ends the cell. A lone backslash before any other character
    // passes the character through verbatim (no other escapes defined).
    static QStringList smartSplit(const QString& line)
    {
        QStringList parts;
        QString current;
        bool prevBackslash = false;
        for (const QChar c : line) {
            if (prevBackslash) {
                // Previous char was backslash — current char is escaped.
                current.append(c);
                prevBackslash = false;
            } else if (c == QLatin1Char('\\')) {
                prevBackslash = true;
            } else if (c == QLatin1Char(',')) {
                parts.append(current);
                current.clear();
            } else {
                current.append(c);
            }
        }
        // Trailing backslash with no following char: pass it through.
        if (prevBackslash) {
            current.append(QLatin1Char('\\'));
        }
        parts.append(current);
        return parts;
    }

private:
    QList<Row> loadMatrix()
    {
        QFile f(QStringLiteral(NEREUS_TEST_DATA_DIR "/tci/matrix.csv"));
        if (!f.open(QIODevice::ReadOnly)) {
            return {};
        }
        QTextStream ts(&f);
        ts.readLine();   // skip header
        QList<Row> rows;
        while (!ts.atEnd()) {
            // Phase 6: use smartSplit to handle \, escaped commas in cells.
            const QStringList parts = smartSplit(ts.readLine());
            if (parts.size() < 6) {
                continue;
            }
            rows.append({parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]});
        }
        return rows;
    }

private slots:
    void allRowsPass()
    {
        TestMockRadioModel mock;
        TciProtocol p(&mock);
        for (const auto& r : loadMatrix()) {
            mock.resetToBaseline();

            // SETUP: prefix handling for AppSettings preconditions plus
            // command-chain support (Phase 6).
            // Multiple segments separated by ";;" within the input cell:
            //   - SETUP:Key=Value  → applied to AppSettings.
            //   - Any other string → dispatched as a command; response and
            //     notifications are drained and discarded so they don't leak
            //     into the final assertion.
            // The last ";;"-separated segment is the command under test.
            QStringList parts = r.input.split(QStringLiteral(";;"));
            for (int i = 0; i < parts.size() - 1; ++i) {
                if (parts[i].startsWith(QStringLiteral("SETUP:"))) {
                    const QString kv = parts[i].mid(6);
                    const int eq = kv.indexOf(QLatin1Char('='));
                    AppSettings::instance().setValue(kv.left(eq), kv.mid(eq + 1));
                } else {
                    // Phase 6: dispatch non-SETUP early segments as commands
                    // so set;;query chain patterns work (e.g. vfo:0,0,14250000;;vfo:0,0;).
                    p.handleCommand(parts[i]);
                    // Phase 15: drain coalesced VFO updates before checking
                    // notifications (synchronous test model — no 5ms timer here).
                    p.drainCoalescedNotifications();
                    // Drain notifications from the early segment so they don't
                    // accumulate into the final assertion.
                    while (p.hasPendingNotification()) {
                        p.takePendingNotification();
                    }
                }
            }

            const QString actualResponse = p.handleCommand(parts.last());
            // Phase 15: drain coalesced VFO updates into m_pendingNotifications
            // before asserting on the notification queue. The matrix runner is
            // synchronous (no 5ms drain timer); this call replicates the tick.
            p.drainCoalescedNotifications();
            QStringList notifs;
            while (p.hasPendingNotification()) {
                notifs.append(p.takePendingNotification());
            }
            QCOMPARE(actualResponse, r.expectedResponse);
            QCOMPARE(notifs.join(QStringLiteral("|")), r.expectedNotifs);
        }
    }
};

QTEST_GUILESS_MAIN(TestTciMatrixRunner)
#include "tst_tci_matrix_runner.moc"
