// no-port-check: NereusSDR-original bounded native CAT log viewer.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-07 Reads CatControl, so a connected desktop shows the Core's CAT
//            log. J.J. Boyd (KG4VCF), AI tooling: Claude Code.
#pragma once
#include "core/cat/CatControl.h"
#include <QDialog>
#include <QPointer>
#include <QVector>
class QPlainTextEdit; class QPushButton; class QCheckBox; class QComboBox;
namespace NereusSDR {
class CatLogWindow : public QDialog {
    Q_OBJECT
public:
    explicit CatLogWindow(CatControl*,QWidget* parent=nullptr);
    ~CatLogWindow() override;
protected:
    void closeEvent(QCloseEvent*) override;
private:
    struct Entry { int direction; QString line; };
    QVector<Entry> m_entries;
    QPlainTextEdit* m_view{}; QPushButton* m_pause{}; QCheckBox* m_scroll{}; QComboBox* m_filter{};
    QPointer<CatControl> m_control;
    QMetaObject::Connection m_logged;
    // The state last shown, so each change is one diagnostic line.
    QVector<CatChannelStatus> m_lastStatus;
    QString m_lastPtt;
    void append(int,const QString&);
    void refresh();
    void noteChanges();
};
}
