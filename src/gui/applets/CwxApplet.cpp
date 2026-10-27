// =================================================================
// src/gui/applets/CwxApplet.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Port of AetherSDR `src/gui/CwxPanel.{h,cpp}` (CW text
//                 entry + WPM + message-slot buttons). Renamed to
//                 CwxApplet in NereusSDR. All controls NYI.
//   2026-10-26: CW transmit, the applet wired to the CWX sender (R-CW-44):
//                 Send, Stop and Key, Speed, M1 to M8 with right-click
//                 edit, Rpt and its delay, KB to CW, sent and current
//                 characters; the NYI overlays removed. J.J. Boyd
//                 (KG4VCF), AI-assisted via Claude Code.
//   2026-10-26: CW transmit, remote CW (R-CW-49): setRemoteSender, so a
//                 remote window's Send, memories, Stop, Key and KB to CW go
//                 to the Core's CWX sender, and showRefusal. J.J. Boyd
//                 (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "CwxApplet.h"
#include "core/CwxSender.h"
#include "core/session/RemoteTransmitClient.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace NereusSDR {

namespace {

// The mockup's marks (cw-mockup.html .cwtext .sent and .cur): sent
// characters at 45 percent, the current one white on the CWX green.
constexpr double kSentOpacity = 0.45;
constexpr auto kCurrentBackground = "#2f6146";
constexpr auto kCurrentText = "#ffffff";

using MemoryGetter = QString (TransmitModel::*)() const;
using MemorySetter = void (TransmitModel::*)(const QString&);
constexpr MemoryGetter kMemoryGetters[CwxSender::kMemoryCount] = {
    &TransmitModel::cwxMemory1, &TransmitModel::cwxMemory2, &TransmitModel::cwxMemory3,
    &TransmitModel::cwxMemory4, &TransmitModel::cwxMemory5, &TransmitModel::cwxMemory6,
    &TransmitModel::cwxMemory7, &TransmitModel::cwxMemory8};
constexpr MemorySetter kMemorySetters[CwxSender::kMemoryCount] = {
    &TransmitModel::setCwxMemory1, &TransmitModel::setCwxMemory2, &TransmitModel::setCwxMemory3,
    &TransmitModel::setCwxMemory4, &TransmitModel::setCwxMemory5, &TransmitModel::setCwxMemory6,
    &TransmitModel::setCwxMemory7, &TransmitModel::setCwxMemory8};

// Escape in the text stops sending, as the mockup's text field does.
class EscapeStops : public QObject {
public:
    EscapeStops(QObject* parent, std::function<void()> stop)
        : QObject(parent), m_stop(std::move(stop)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            m_stop();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_stop;
};

} // namespace

CwxApplet::CwxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    wireSender();
    wireModel();
    syncFromModel();
    applyPermissions();
}

CwxSender* CwxApplet::sender() const
{
    return m_model ? m_model->cwxSender() : nullptr;
}

void CwxApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: Text input (60px, dark styled) ---
    m_textEdit = new QTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("cwxText"));
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setFixedHeight(60);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px; color: %3; font-size: 11px;"
        "}").arg(Style::kInsetBg, Style::kInsetBorder, Style::kTextPrimary));
    m_textEdit->setAccessibleName(QStringLiteral("CWX text"));
    m_textEdit->setToolTip(QStringLiteral("Type the text to send. Escape stops sending."));
    m_textEdit->installEventFilter(new EscapeStops(m_textEdit, [this]() { stopSending(); }));
    vbox->addWidget(m_textEdit);

    // --- Control 2: the Send row. Stop and Key sit over M3 and M4, on
    // their columns (cw-mockup.html, "Stop and Key added on the Send row").
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);
        m_sendBtn = greenToggle(QStringLiteral("Send"));
        m_sendBtn->setObjectName(QStringLiteral("cwxSend"));
        m_sendBtn->setCheckable(true);
        row->addWidget(m_sendBtn, 2);
        m_stopBtn = styledButton(QStringLiteral("Stop"));
        m_stopBtn->setObjectName(QStringLiteral("cwxStop"));
        m_stopBtn->setToolTip(QStringLiteral("Stop sending and clear what is left"));
        row->addWidget(m_stopBtn, 1);
        m_keyBtn = greenToggle(QStringLiteral("Key"));
        m_keyBtn->setObjectName(QStringLiteral("cwxKey"));
        m_keyBtn->setCheckable(true);
        row->addWidget(m_keyBtn, 1);
        vbox->addLayout(row);
    }

    // --- Control 3: Speed override slider (5..60 WPM) ---
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setObjectName(QStringLiteral("cwxSpeedSlider"));
    m_speedSlider->setRange(TransmitModel::kCwxSpeedMin, TransmitModel::kCwxSpeedMax);
    m_speedSlider->setValue(TransmitModel::kCwxSpeedDefault);
    m_speedSlider->setAccessibleName(QStringLiteral("CWX speed"));
    m_speedSlider->setToolTip(QStringLiteral("CWX speed in words per minute"));
    m_speedValue  = insetValue(QString::number(TransmitModel::kCwxSpeedDefault));
    vbox->addLayout(sliderRow(QStringLiteral("Speed"), m_speedSlider, m_speedValue));

    // --- Control 4: Memory buttons M1-M8 (2 rows of 4) ---
    for (int row = 0; row < 2; ++row) {
        auto* hrow = new QHBoxLayout;
        hrow->setSpacing(2);
        for (int col = 0; col < 4; ++col) {
            const int idx = row * 4 + col;
            m_memBtn[idx] = styledButton(QStringLiteral("M%1").arg(idx + 1));
            m_memBtn[idx]->setObjectName(QStringLiteral("cwxMemory%1").arg(idx + 1));
            m_memBtn[idx]->setContextMenuPolicy(Qt::CustomContextMenu);
            hrow->addWidget(m_memBtn[idx]);
        }
        vbox->addLayout(hrow);
    }

    // --- Control 5: Repeat toggle + interval slider (1..60s) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_repeatBtn = greenToggle(QStringLiteral("Rpt"));
        m_repeatBtn->setObjectName(QStringLiteral("cwxRepeat"));
        m_repeatBtn->setCheckable(true);
        m_repeatBtn->setToolTip(QStringLiteral("Repeat a memory after the delay"));
        row->addWidget(m_repeatBtn);

        m_repeatSlider = new QSlider(Qt::Horizontal, this);
        m_repeatSlider->setObjectName(QStringLiteral("cwxRepeatDelaySlider"));
        m_repeatSlider->setRange(TransmitModel::kCwxRepeatDelaySMin,
                                 TransmitModel::kCwxRepeatDelaySMax);
        m_repeatSlider->setValue(TransmitModel::kCwxRepeatDelaySDefault);
        m_repeatSlider->setFixedHeight(18);
        m_repeatSlider->setAccessibleName(QStringLiteral("CWX repeat delay"));
        m_repeatSlider->setToolTip(QStringLiteral("Seconds between repeats"));
        row->addWidget(m_repeatSlider, 1);

        m_repeatValue = insetValue(QString::number(TransmitModel::kCwxRepeatDelaySDefault));
        row->addWidget(m_repeatValue);

        vbox->addLayout(row);
    }

    // --- Control 6: Keyboard-to-CW toggle ---
    m_kbCwBtn = greenToggle(QStringLiteral("KB→CW"));
    m_kbCwBtn->setObjectName(QStringLiteral("cwxKeyboard"));
    m_kbCwBtn->setCheckable(true);
    vbox->addWidget(m_kbCwBtn);

    vbox->addStretch();
    root->addWidget(body);

    connect(m_sendBtn, &QPushButton::clicked, this, [this]() { send(); });
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() { stopSending(); });
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() { onTextChanged(); });
    for (int i = 0; i < CwxSender::kMemoryCount; ++i) {
        connect(m_memBtn[i], &QPushButton::clicked, this, [this, i]() { sendMemory(i + 1); });
        connect(m_memBtn[i], &QPushButton::customContextMenuRequested, this,
                [this, i](const QPoint&) { editMemory(i + 1); });
    }
    connect(m_kbCwBtn, &QPushButton::toggled, this, [this](bool on) {
        // Only what is typed from now on goes.
        m_kbLength = m_textEdit->toPlainText().length();
        if (on) {
            m_textEdit->setFocus();
        }
        applyPermissions();
    });
}

void CwxApplet::wireSender()
{
    // CW transmit (R-CW-49): the remote Key, lit from the press, dark at
    // the release, a refusal, or the Core's 3 s watchdog.
    m_remoteKeyWatchdog = new QTimer(this);
    m_remoteKeyWatchdog->setSingleShot(true);
    m_remoteKeyWatchdog->setInterval(CwxSender::kKeyWatchdogMs);
    connect(m_remoteKeyWatchdog, &QTimer::timeout, this, [this]() { setRemoteKeyDown(false); });
    CwxSender* const cwx = sender();
    // The Key holds the key while it is pressed; the sender drops a hold
    // whose release never comes 3 s after the press.
    connect(m_keyBtn, &QPushButton::pressed, this, [this, cwx]() {
        if (!m_sendPermitted) {
            return;
        }
        m_lastRefusal.clear();
        if (hasRemoteSender()) {
            const QString words = m_remote.setKey ? m_remote.setKey(true) : QString();
            if (!words.isEmpty()) {
                setRemoteKeyDown(false);
                refuse(words);
                return;
            }
            setRemoteKeyDown(true);
            return;
        }
        if (cwx == nullptr) {
            return;
        }
        cwx->setKeyHeld(true);
        if (!m_lastRefusal.isEmpty()) {
            refuse(m_lastRefusal);
        }
    });
    connect(m_keyBtn, &QPushButton::released, this, [this, cwx]() {
        if (hasRemoteSender()) {
            if (m_remoteKeyDown && m_remote.setKey) {
                m_remote.setKey(false);
            }
            setRemoteKeyDown(false);
            return;
        }
        if (cwx != nullptr) {
            cwx->setKeyHeld(false);
        }
    });
    // The button's lit state is the sender's, not the click's.
    connect(m_keyBtn, &QPushButton::toggled, this, [this, cwx](bool) {
        const QSignalBlocker block(m_keyBtn);
        m_keyBtn->setChecked(hasRemoteSender() ? m_remoteKeyDown
                                               : cwx != nullptr && cwx->isKeyHeld());
    });
    if (cwx == nullptr) {
        return;
    }
    connect(cwx, &CwxSender::keyHeldChanged, this, [this](bool held) {
        if (hasRemoteSender()) {
            return;
        }
        const QSignalBlocker block(m_keyBtn);
        m_keyBtn->setChecked(held);
    });
    connect(cwx, &CwxSender::characterStarted, this, [this](int index) {
        if (!hasRemoteSender()) {
            onCharacterStarted(index);
        }
    });
    connect(cwx, &CwxSender::sendingChanged, this, [this](bool sending) {
        if (!hasRemoteSender()) {
            onSendingChanged(sending);
        }
    });
    connect(cwx, &CwxSender::refused, this, [this](const QString& words) { m_lastRefusal = words; });
}

void CwxApplet::setRemoteSender(RemoteSender remote)
{
    m_remote = std::move(remote);
    clearMarks();
    setRemoteKeyDown(false);
}

CwxApplet::RemoteSender CwxApplet::remoteSenderFor(std::function<RemoteTransmitClient*()> client,
                                                   const QString& newerCoreReason)
{
    const auto request = [client = std::move(client), newerCoreReason](
                             const std::function<quint32(RemoteTransmitClient&)>& call) {
        RemoteTransmitClient* const transmit = client ? client() : nullptr;
        if (transmit == nullptr || !transmit->available()) {
            return QString::fromLatin1(RemoteTransmitClient::kNoLinkReason);
        }
        if (!transmit->cwAvailable()) {
            return newerCoreReason;
        }
        return call(*transmit) != 0 ? QString()
                                    : QString::fromLatin1(RemoteTransmitClient::kNoLinkReason);
    };
    RemoteSender remote;
    remote.sendText = [request](const QString& text) {
        return request([&text](RemoteTransmitClient& c) { return c.sendCwText(text); });
    };
    remote.sendMemory = [request](int number) {
        return request([number](RemoteTransmitClient& c) { return c.sendCwMemory(number); });
    };
    remote.stop = [request]() {
        return request([](RemoteTransmitClient& c) { return c.stopCw(); });
    };
    remote.setKey = [request](bool down) {
        return request([down](RemoteTransmitClient& c) { return c.setCwKey(down); });
    };
    return remote;
}

void CwxApplet::setRemoteKeyDown(bool down)
{
    m_remoteKeyDown = down;
    if (m_remoteKeyWatchdog != nullptr) {
        if (down) {
            m_remoteKeyWatchdog->start();
        } else {
            m_remoteKeyWatchdog->stop();
        }
    }
    if (hasRemoteSender()) {
        const QSignalBlocker block(m_keyBtn);
        m_keyBtn->setChecked(down);
    }
}

void CwxApplet::showRefusal(const QString& words)
{
    if (hasRemoteSender()) {
        setRemoteKeyDown(false);
    }
    refuse(words);
}

void CwxApplet::wireModel()
{
    if (m_model == nullptr) {
        return;
    }
    TransmitModel& tm = m_model->transmitModel();
    connect(m_speedSlider, &QSlider::valueChanged, this, [this, &tm](int wpm) {
        m_speedValue->setText(QString::number(wpm));
        if (!m_updatingFromModel) {
            tm.setCwxSpeed(wpm);
        }
    });
    connect(m_repeatBtn, &QPushButton::toggled, this, [this, &tm](bool on) {
        if (!m_updatingFromModel) {
            tm.setCwxRepeat(on);
        }
    });
    connect(m_repeatSlider, &QSlider::valueChanged, this, [this, &tm](int seconds) {
        m_repeatValue->setText(QString::number(seconds));
        if (!m_updatingFromModel) {
            tm.setCwxRepeatDelayS(seconds);
        }
    });
    connect(&tm, &TransmitModel::cwxSpeedChanged, this, [this]() { syncFromModel(); });
    connect(&tm, &TransmitModel::cwxRepeatChanged, this, [this]() { syncFromModel(); });
    connect(&tm, &TransmitModel::cwxRepeatDelaySChanged, this, [this]() { syncFromModel(); });
    for (auto signal : {&TransmitModel::cwxMemory1Changed, &TransmitModel::cwxMemory2Changed,
                        &TransmitModel::cwxMemory3Changed, &TransmitModel::cwxMemory4Changed,
                        &TransmitModel::cwxMemory5Changed, &TransmitModel::cwxMemory6Changed,
                        &TransmitModel::cwxMemory7Changed, &TransmitModel::cwxMemory8Changed}) {
        connect(&tm, signal, this, [this]() { updateMemoryTooltips(); });
    }
}

void CwxApplet::syncFromModel()
{
    if (m_model == nullptr) {
        return;
    }
    const TransmitModel& tm = m_model->transmitModel();
    m_updatingFromModel = true;
    m_speedSlider->setValue(tm.cwxSpeed());
    m_speedValue->setText(QString::number(tm.cwxSpeed()));
    m_repeatBtn->setChecked(tm.cwxRepeat());
    m_repeatSlider->setValue(tm.cwxRepeatDelayS());
    m_repeatValue->setText(QString::number(tm.cwxRepeatDelayS()));
    m_updatingFromModel = false;
    updateMemoryTooltips();
}

QString CwxApplet::memoryText(int number) const
{
    if (m_model == nullptr || number < 1 || number > CwxSender::kMemoryCount) {
        return {};
    }
    return (m_model->transmitModel().*kMemoryGetters[number - 1])();
}

void CwxApplet::setMemoryText(int number, const QString& text)
{
    if (m_model == nullptr || number < 1 || number > CwxSender::kMemoryCount) {
        return;
    }
    (m_model->transmitModel().*kMemorySetters[number - 1])(text);
}

void CwxApplet::updateMemoryTooltips()
{
    for (int i = 0; i < CwxSender::kMemoryCount; ++i) {
        const QString text = memoryText(i + 1);
        QString tip = text.trimmed().isEmpty()
            ? QStringLiteral("M%1 is empty (right-click to edit)").arg(i + 1)
            : QStringLiteral("%1 (right-click to edit)").arg(text);
        if (!m_sendPermitted && !m_sendReason.isEmpty()) {
            tip = m_sendReason;
        }
        m_memBtn[i]->setToolTip(tip);
        m_memBtn[i]->setAccessibleDescription(tip);
    }
}

void CwxApplet::setSendPermitted(bool permitted, const QString& reason)
{
    m_sendPermitted = permitted;
    m_sendReason = permitted ? QString() : reason;
    if (!permitted && hasRemoteSender()) {
        // The Core ends a device's CWX itself when it may no longer send
        // (another device takes transmit, its link drops); a stop from
        // here would only be refused with the same news.
        setRemoteKeyDown(false);
        clearMarks();
    } else if (!permitted) {
        stopSending();
    }
    applyPermissions();
}

void CwxApplet::setCwxSettingsPermitted(bool permitted, const QString& reason)
{
    m_settingsPermitted = permitted;
    m_settingsReason = permitted ? QString() : reason;
    applyPermissions();
}

void CwxApplet::applyPermissions()
{
    const bool kb = m_kbCwBtn->isChecked();
    const auto apply = [](QWidget* w, bool enabled, const QString& tip) {
        w->setEnabled(enabled);
        w->setToolTip(tip);
        w->setAccessibleDescription(enabled ? QString() : tip);
    };
    const QString sendReason = m_sendReason;
    if (!m_sendPermitted) {
        apply(m_sendBtn, false, sendReason);
        apply(m_stopBtn, false, sendReason);
        apply(m_keyBtn, false, sendReason);
        apply(m_kbCwBtn, false, sendReason);
    } else {
        apply(m_sendBtn, !kb, kb ? QStringLiteral("KB to CW is on: what you type is sent as you "
                                                  "type it.")
                                 : QStringLiteral("Send the text"));
        apply(m_stopBtn, true, QStringLiteral("Stop sending and clear what is left"));
        // The mockup's Key tooltip.
        apply(m_keyBtn, true, QStringLiteral("Hand key: transmits while held, released after 3 s"));
        apply(m_kbCwBtn, true, QStringLiteral("KB to CW: each character goes as you type it"));
    }
    for (QPushButton* mem : m_memBtn) {
        mem->setEnabled(m_sendPermitted);
    }
    updateMemoryTooltips();

    const QString settingsReason = m_settingsReason;
    apply(m_speedSlider, m_settingsPermitted,
          m_settingsPermitted ? QStringLiteral("CWX speed in words per minute") : settingsReason);
    apply(m_repeatBtn, m_settingsPermitted,
          m_settingsPermitted ? QStringLiteral("Repeat a memory after the delay") : settingsReason);
    apply(m_repeatSlider, m_settingsPermitted,
          m_settingsPermitted ? QStringLiteral("Seconds between repeats") : settingsReason);
}

void CwxApplet::editMemory(int number)
{
    if (number < 1 || number > CwxSender::kMemoryCount) {
        return;
    }
    if (!m_settingsPermitted) {
        refuse(m_settingsReason);
        return;
    }
    if (m_memoryEditor != nullptr) {
        m_memoryEditor->close();
        m_memoryEditor->deleteLater();
        m_memoryEditor = nullptr;
    }
    auto* editor = new QInputDialog(this);
    editor->setObjectName(QStringLiteral("cwxMemoryEditor"));
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->setWindowTitle(QStringLiteral("CWX memory M%1").arg(number));
    editor->setLabelText(QStringLiteral("Text for M%1:").arg(number));
    editor->setInputMode(QInputDialog::TextInput);
    editor->setTextValue(memoryText(number));
    connect(editor, &QInputDialog::textValueSelected, this,
            [this, number](const QString& text) { setMemoryText(number, text); });
    connect(editor, &QObject::destroyed, this, [this, editor]() {
        if (m_memoryEditor == editor) {
            m_memoryEditor = nullptr;
        }
    });
    m_memoryEditor = editor;
    editor->open();
}

void CwxApplet::refuse(const QString& words)
{
    if (words.isEmpty()) {
        return;
    }
    m_lastRefusal = words;
    emit requestRefused(words);
}

void CwxApplet::send()
{
    // The button lights from the sender, not from the click.
    {
        const QSignalBlocker block(m_sendBtn);
        m_sendBtn->setChecked(m_sending);
    }
    if (hasRemoteSender()) {
        if (!m_sendPermitted || m_kbCwBtn->isChecked()) {
            return;
        }
        // Send starts the text over at the Core too: what it has left
        // stops first (the link keeps the order).
        QString words = m_remote.stop ? m_remote.stop() : QString();
        if (words.isEmpty()) {
            words = m_remote.sendText(m_textEdit->toPlainText());
        }
        refuse(words);
        return;
    }
    CwxSender* const cwx = sender();
    if (cwx == nullptr || !m_sendPermitted || m_kbCwBtn->isChecked()) {
        return;
    }
    const QString text = m_textEdit->toPlainText();
    // Send starts the text over, as the mockup's Send does.
    if (m_sending) {
        cwx->stop();
        clearMarks();
    }
    m_lastRefusal.clear();
    if (!cwx->sendText(text)) {
        refuse(m_lastRefusal);
        return;
    }
    m_requests = {Request{0, static_cast<int>(text.length())}};
    m_requestIndex = -1;
    m_lastIndex = -1;
    m_sentFrom = 0;
}

void CwxApplet::sendMemory(int number)
{
    CwxSender* const cwx = sender();
    if ((cwx == nullptr && !hasRemoteSender()) || !m_sendPermitted) {
        return;
    }
    const QString text = memoryText(number);
    m_lastRefusal.clear();
    if (hasRemoteSender()) {
        // The Core sends its own copy of the memory (the mirrored text).
        if (const QString words = m_remote.sendMemory ? m_remote.sendMemory(number) : QString();
            !words.isEmpty()) {
            refuse(words);
            return;
        }
        clearMarks();
        m_editingText = true;
        m_textEdit->setPlainText(text);
        m_editingText = false;
        m_kbLength = static_cast<int>(text.length());
        return;
    }
    if (!cwx->sendMemory(number)) {
        refuse(m_lastRefusal);
        return;
    }
    // The memory's text goes in the field, where its characters are marked
    // as they go (the mockup's memory buttons).
    clearMarks();
    m_editingText = true;
    m_textEdit->setPlainText(text);
    m_editingText = false;
    m_kbLength = static_cast<int>(text.length());
    m_requests = {Request{0, static_cast<int>(text.length())}};
    m_requestIndex = -1;
    m_lastIndex = -1;
    m_sentFrom = 0;
}

void CwxApplet::stopSending()
{
    if (hasRemoteSender()) {
        if (m_remote.stop) {
            refuse(m_remote.stop());
        }
        setRemoteKeyDown(false);
    } else if (CwxSender* const cwx = sender()) {
        cwx->stop();
    }
    clearMarks();
}

void CwxApplet::onTextChanged()
{
    if (m_editingText) {
        return;
    }
    const QString text = m_textEdit->toPlainText();
    if (!m_kbCwBtn->isChecked() || !m_sendPermitted) {
        m_kbLength = static_cast<int>(text.length());
        return;
    }
    // KB to CW: what was added at the end goes as typed. An edit anywhere
    // else sends nothing (a sent character cannot be taken back).
    const int previous = m_kbLength;
    m_kbLength = static_cast<int>(text.length());
    if (text.length() <= previous) {
        return;
    }
    const QString added = text.mid(previous);
    if (hasRemoteSender()) {
        refuse(m_remote.sendText(added));
        return;
    }
    CwxSender* const cwx = sender();
    if (cwx == nullptr) {
        return;
    }
    m_lastRefusal.clear();
    if (!cwx->sendText(added)) {
        refuse(m_lastRefusal);
        return;
    }
    if (m_requests.isEmpty()) {
        m_requestIndex = -1;
        m_lastIndex = -1;
        m_sentFrom = previous;
    }
    m_requests.append(Request{previous, static_cast<int>(added.length())});
}

// characterStarted carries the index in its request's text. Requests go in
// order: an index past the last one in the current request is the same
// request; anything else is the next request, or, with none queued, the
// same one starting over (a repeating memory).
void CwxApplet::onCharacterStarted(int index)
{
    if (m_requests.isEmpty() || index < 0) {
        return;
    }
    if (m_requestIndex < 0) {
        m_requestIndex = 0;
    } else {
        const Request& current = m_requests.at(m_requestIndex);
        const bool sameRequest = index > m_lastIndex && index < current.length;
        if (!sameRequest && m_requestIndex + 1 < m_requests.size()) {
            ++m_requestIndex;
        }
    }
    m_lastIndex = index;
    const Request& request = m_requests.at(m_requestIndex);
    m_markedCurrent = request.base + index;
    applyMarks();
}

void CwxApplet::onSendingChanged(bool sending)
{
    m_sending = sending;
    {
        const QSignalBlocker block(m_sendBtn);
        m_sendBtn->setChecked(sending && !m_kbCwBtn->isChecked());
    }
    if (!sending) {
        clearMarks();
    }
}

void CwxApplet::clearMarks()
{
    m_requests.clear();
    m_requestIndex = -1;
    m_lastIndex = -1;
    m_markedCurrent = -1;
    m_textEdit->setExtraSelections({});
}

void CwxApplet::applyMarks()
{
    const int length = static_cast<int>(m_textEdit->toPlainText().length());
    if (m_markedCurrent < 0 || m_markedCurrent >= length) {
        m_textEdit->setExtraSelections({});
        return;
    }
    QList<QTextEdit::ExtraSelection> marks;
    if (m_markedCurrent > m_sentFrom) {
        QTextEdit::ExtraSelection sent;
        sent.cursor = QTextCursor(m_textEdit->document());
        sent.cursor.setPosition(m_sentFrom);
        sent.cursor.setPosition(m_markedCurrent, QTextCursor::KeepAnchor);
        QColor dim(Style::kTextPrimary);
        dim.setAlphaF(kSentOpacity);
        sent.format.setForeground(dim);
        marks.append(sent);
    }
    QTextEdit::ExtraSelection current;
    current.cursor = QTextCursor(m_textEdit->document());
    current.cursor.setPosition(m_markedCurrent);
    current.cursor.setPosition(m_markedCurrent + 1, QTextCursor::KeepAnchor);
    current.format.setBackground(QColor(kCurrentBackground));
    current.format.setForeground(QColor(kCurrentText));
    marks.append(current);
    m_textEdit->setExtraSelections(marks);
}

} // namespace NereusSDR
