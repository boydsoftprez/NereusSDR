// no-port-check: NereusSDR-original native log mechanics; follows existing TciLogWindow pattern.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-07 Reads CatControl: the bytes from logged(), the diagnostics from
//            each change of state, so a connected desktop shows the Core's
//            CAT log; review fixes: the Core's recent lines when it opens,
//            each line at the time CAT saw it, diagnostics from the state
//            changes only. J.J. Boyd (KG4VCF), AI tooling: Claude Code.
#include "CatLogWindow.h"
#include "core/AppSettings.h"
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
// A connected desktop is sent as many of the Core's recent lines.
constexpr int kMaximumEntries=CatControl::kLogLines;
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
CatLogWindow::CatLogWindow(CatControl* control,QWidget* parent) : QDialog(parent), m_control(control) {
    setWindowTitle(tr("CAT Log")); setObjectName("CatLogWindow"); setModal(false); setAttribute(Qt::WA_DeleteOnClose,false);
    setStyleSheet(QString::fromLatin1(Style::kPageStyle)); resize(720,480);
    // Settings store the geometry as a base64 latin1 string, as TciLogWindow does (XML-safe).
    const QByteArray geometry=QByteArray::fromBase64(AppSettings::instance().value("CatLogWindowGeometry",QString{}).toString().toLatin1()); if (!geometry.isEmpty()) { restoreGeometry(geometry); }
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
    if (control) {
        // A connected desktop follows the Core's CAT log while this listens.
        m_logged=connect(control,&CatControl::logged,this,[this](int channel,bool inbound,const QByteArray& bytes,qint64 timeMs) {
            append(inbound ? 1:2,tr("CAT%1 %2 bytes=%3  %4  [hex %5]").arg(channel).arg(inbound ? "in":"out").arg(bytes.size()).arg(escaped(bytes),QString::fromLatin1(bytes.toHex(' '))),timeMs);
        });
        for (int channel=1;channel<=4;++channel) { m_lastStatus.append(control->channelStatus(channel)); }
        m_lastPtt=control->pttState();
        connect(control,&CatControl::channelStatusChanged,this,&CatLogWindow::noteChanges);
        connect(control,&CatControl::pttStateChanged,this,&CatLogWindow::noteChanges);
    }
}
CatLogWindow::~CatLogWindow() { disconnect(m_logged); }
void CatLogWindow::noteChanges() {
    if (!m_control) { return; }
    for (int channel=1;channel<=m_lastStatus.size();++channel) {
        const CatChannelStatus now=m_control->channelStatus(channel); CatChannelStatus& was=m_lastStatus[channel-1];
        const auto transport=[&](const QString& name,const QString& state,const QString& before) { if (state!=before) { append(3,tr("CAT%1 %2: %3").arg(channel).arg(name,state)); } };
        transport(tr("TCP"),now.tcp,was.tcp); transport(tr("Serial"),now.serial,was.serial); transport(tr("PTY"),now.pty,was.pty); transport(tr("Rigctld"),now.rigctld,was.rigctld);
        if (now.state!=was.state) { append(3,tr("CAT%1 state: %2").arg(channel).arg(now.state)); }
        if (now.tcpClients!=was.tcpClients) { append(3,tr("CAT%1 TCP clients: %2").arg(channel).arg(now.tcpClients)); }
        was=now;
    }
    const QString ptt=m_control->pttState();
    if (ptt!=m_lastPtt) { m_lastPtt=ptt; append(3,tr("PTT: %1").arg(ptt)); }
}
void CatLogWindow::append(int direction,const QString& line,qint64 timeMs) {
    if (m_pause->isChecked()) { return; }
    const QDateTime when=timeMs>0 ? QDateTime::fromMSecsSinceEpoch(timeMs) : QDateTime::currentDateTime();
    const QString dated=when.toString("HH:mm:ss.zzz")+"  "+line;
    m_entries.append({direction,dated}); if (m_entries.size()>kMaximumEntries) { m_entries.removeFirst(); }
    if (m_filter->currentIndex()==0 || m_filter->currentIndex()==direction) { m_view->appendPlainText(dated); }
    if (m_scroll->isChecked()) { m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum()); }
}
void CatLogWindow::refresh() {
    m_view->clear(); for (const Entry& entry:m_entries) { if (m_filter->currentIndex()==0 || entry.direction==m_filter->currentIndex()) { m_view->appendPlainText(entry.line); } }
}
void CatLogWindow::closeEvent(QCloseEvent* event) { AppSettings::instance().setValue("CatLogWindowGeometry",QString::fromLatin1(saveGeometry().toBase64())); QDialog::closeEvent(event); }
}
