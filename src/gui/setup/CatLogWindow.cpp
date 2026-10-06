// no-port-check: NereusSDR-original native log mechanics; follows existing TciLogWindow pattern.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatLogWindow.h"
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#include "gui/StyleConstants.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>
namespace NereusSDR {
namespace {
constexpr int kMaximumEntries=10000;
QString escaped(const QByteArray& bytes) {
    QString text;
    for (const char byte:bytes) {
        const unsigned char value=static_cast<unsigned char>(byte);
        if (value>=32 && value<=126 && byte!='\\') { text+=QChar(value); }
        else { text+=QStringLiteral("\\x%1").arg(value,2,16,QLatin1Char('0')); }
    }
    return text;
}
}
CatLogWindow::CatLogWindow(CatService* service,QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("CAT Log")); setObjectName("CatLogWindow"); setModal(false); setAttribute(Qt::WA_DeleteOnClose,false);
    setStyleSheet(QString::fromLatin1(Style::kPageStyle)); resize(720,480);
    const QByteArray geometry=AppSettings::instance().value("CatLogWindowGeometry").toByteArray(); if (!geometry.isEmpty()) { restoreGeometry(geometry); }
    auto* root=new QVBoxLayout(this); auto* toolbar=new QHBoxLayout;
    m_scroll=new QCheckBox(tr("Auto-scroll"),this); m_scroll->setChecked(true); toolbar->addWidget(m_scroll);
    m_pause=new QPushButton(tr("Pause"),this); m_pause->setObjectName("catLogPause"); m_pause->setCheckable(true); m_pause->setAutoDefault(false);
    m_pause->setToolTip(tr("Drop new displayed entries while paused. Resume starts with current activity; dropped traffic is not replayed.")); toolbar->addWidget(m_pause);
    auto* clear=new QPushButton(tr("Clear"),this); clear->setAutoDefault(false); clear->setObjectName("catLogClear"); toolbar->addWidget(clear);
    toolbar->addStretch(); m_filter=new QComboBox(this); m_filter->setObjectName("catLogFilter"); m_filter->addItems({tr("All"),tr("Requests"),tr("Replies"),tr("Diagnostics")}); toolbar->addWidget(m_filter);
    root->addLayout(toolbar); m_view=new QPlainTextEdit(this); m_view->setObjectName("catLogText"); m_view->setReadOnly(true); m_view->setMaximumBlockCount(kMaximumEntries);
    QFont font("Menlo"); font.setStyleHint(QFont::TypeWriter); font.setPointSize(10); m_view->setFont(font);
    m_view->setStyleSheet("QPlainTextEdit { background: #1a1a1a; color: #d0d0d0; border: 1px solid #3a3a3a; }"); root->addWidget(m_view,1);
    connect(clear,&QPushButton::clicked,this,[this] { m_entries.clear(); m_view->clear(); }); connect(m_filter,&QComboBox::currentIndexChanged,this,[this] { refresh(); });
    if (service) {
        connect(service,&CatService::messageLogged,this,[this](int channel,bool inbound,const QByteArray& bytes) {
            append(inbound ? 1:2,tr("CAT%1 %2 bytes=%3  %4  [hex %5]").arg(channel).arg(inbound ? "in":"out").arg(bytes.size()).arg(escaped(bytes),QString::fromLatin1(bytes.toHex(' '))));
        });
        connect(service,&CatService::transportStateChanged,this,[this](int channel,CatTransportKind kind,const QString& state) {
            const QString transport=kind==CatTransportKind::Tcp ? tr("TCP") : kind==CatTransportKind::Serial ? tr("Serial") : kind==CatTransportKind::Pty ? tr("PTY") : tr("Rigctld"); append(3,tr("CAT%1 %2: %3").arg(channel).arg(transport,state));
        });
        connect(service,&CatService::channelStateChanged,this,[this](int channel,const QString& state) { append(3,tr("CAT%1 state: %2").arg(channel).arg(state)); });
        connect(service,&CatService::clientCountChanged,this,[this](int channel,int count) { append(3,tr("CAT%1 TCP clients: %2").arg(channel).arg(count)); });
        connect(service,&CatService::pttStateChanged,this,[this](const QString& state) { append(3,tr("PTT: %1").arg(state)); });
    }
}
void CatLogWindow::append(int direction,const QString& line) {
    if (m_pause->isChecked()) { return; }
    const QString dated=QDateTime::currentDateTime().toString("HH:mm:ss.zzz")+"  "+line;
    m_entries.append({direction,dated}); if (m_entries.size()>kMaximumEntries) { m_entries.removeFirst(); }
    if (m_filter->currentIndex()==0 || m_filter->currentIndex()==direction) { m_view->appendPlainText(dated); }
    if (m_scroll->isChecked()) { m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum()); }
}
void CatLogWindow::refresh() {
    m_view->clear(); for (const Entry& entry:m_entries) { if (m_filter->currentIndex()==0 || entry.direction==m_filter->currentIndex()) { m_view->appendPlainText(entry.line); } }
}
void CatLogWindow::closeEvent(QCloseEvent* event) { AppSettings::instance().setValue("CatLogWindowGeometry",saveGeometry()); QDialog::closeEvent(event); }
}
