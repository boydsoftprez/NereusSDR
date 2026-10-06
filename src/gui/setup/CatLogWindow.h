// no-port-check: NereusSDR-original bounded native CAT log viewer.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include <QDialog>
#include <QVector>
class QPlainTextEdit; class QPushButton; class QCheckBox; class QComboBox;
namespace NereusSDR {
class CatService;
class CatLogWindow : public QDialog {
    Q_OBJECT
public:
    explicit CatLogWindow(CatService*,QWidget* parent=nullptr);
protected:
    void closeEvent(QCloseEvent*) override;
private:
    struct Entry { int direction; QString line; };
    QVector<Entry> m_entries;
    QPlainTextEdit* m_view{}; QPushButton* m_pause{}; QCheckBox* m_scroll{}; QComboBox* m_filter{};
    void append(int,const QString&);
    void refresh();
};
}
