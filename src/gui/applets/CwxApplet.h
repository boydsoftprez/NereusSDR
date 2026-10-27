// =================================================================
// src/gui/applets/CwxApplet.h  (NereusSDR)
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

#pragma once
#include "AppletWidget.h"

#include <QString>
#include <QVector>

#include <functional>

class QPushButton;
class QTextEdit;
class QSlider;
class QLabel;
class QInputDialog;
class QTimer;

namespace NereusSDR {

class CwxSender;
class RemoteTransmitClient;

// CWX: the CW text and memory keyer (R-CW-44). Text, memories and the Key
// go to RadioModel::cwxSender(), which times the elements on its own
// thread; nothing here keys the radio itself.
//
// Controls:
//   1. Text input field     — QTextEdit, 60px height, dark styled; the sent
//                             characters dim and the current one is marked
//   2. Send row             — Send, Stop and Key (Key holds while pressed)
//   3. Speed override       — QSlider (5..60 WPM) + inset, TransmitModel::cwxSpeed
//   4. Memory buttons       — QPushButton x8 ("M1"-"M8"), 2 rows of 4;
//                             right-click edits TransmitModel::cwxMemoryN
//   5. Repeat toggle+interval — "Rpt" + QSlider (1..60 s) + inset
//   6. Keyboard-to-CW toggle — "KB→CW": typed characters go as typed
class CwxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CwxApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("cwx"); }
    QString appletTitle() const override { return QStringLiteral("CWX"); }
    void    syncFromModel() override;

    // A remote window (until its Core can take CWX requests) greys Send,
    // Stop, Key, the memories' send and KB to CW with `reason`.
    void setSendPermitted(bool permitted, const QString& reason = QString());
    // The CWX settings (speed, repeat, memory texts) need the Core's
    // transmit settings; greyed with `reason` when they are not there.
    void setCwxSettingsPermitted(bool permitted, const QString& reason = QString());

    // CW transmit (R-CW-49): a remote window's requests go to the Core's
    // CWX sender. Each returns empty when the request went, or the words it
    // could not go with; the Core's refusal comes back to showRefusal.
    // Set, the window's own sender is never used. The Core does not send
    // its progress, so a remote applet marks no sent or current characters.
    struct RemoteSender {
        std::function<QString(const QString& text)> sendText;
        std::function<QString(int number)> sendMemory;
        std::function<QString()> stop;
        std::function<QString(bool down)> setKey;
    };
    void setRemoteSender(RemoteSender remote);
    // The requests through a remote window's transmit client (cw.send,
    // cw.memory, cw.stop, cw.key), looked up at each request so a window
    // that reconnects keeps working. A client the Core has not told
    // remoteCwVersion answers `newerCoreReason`; no link, or a request that
    // could not go, answers RemoteTransmitClient::kNoLinkReason.
    static RemoteSender remoteSenderFor(std::function<RemoteTransmitClient*()> client,
                                        const QString& newerCoreReason);
    bool hasRemoteSender() const { return static_cast<bool>(m_remote.sendText); }
    // Shows a refusal as the applet's own (requestRefused); the Key goes
    // dark, since the Core did not take it.
    void showRefusal(const QString& words);

    // Opens the editor for memory `number` (1..8), as a right-click does.
    void editMemory(int number);

    // Test seams: the sent characters are [sentFrom, markedCurrent) and the
    // current one is markedCurrent, -1 when nothing is being sent.
    int sentFromForTest() const { return m_sentFrom; }
    int markedCurrentForTest() const { return m_markedCurrent; }
    QString lastRefusalForTest() const { return m_lastRefusal; }
    bool remoteKeyDownForTest() const { return m_remoteKeyDown; }

signals:
    // A Send, memory or Key request was refused, with its words.
    void requestRefused(const QString& words);

private:
    struct Request {
        int base{0};     // where the request's text starts in the field
        int length{0};
    };

    void buildUI();
    void wireSender();
    void wireModel();
    void applyPermissions();
    void send();
    void sendMemory(int number);
    void stopSending();
    void onTextChanged();
    void onCharacterStarted(int index);
    void onSendingChanged(bool sending);
    void clearMarks();
    void applyMarks();
    void refuse(const QString& words);
    void updateMemoryTooltips();
    QString memoryText(int number) const;
    void setMemoryText(int number, const QString& text);
    CwxSender* sender() const;
    void setRemoteKeyDown(bool down);

    // Control 1 — text input
    QTextEdit*   m_textEdit      = nullptr;
    // Control 2 — the Send row
    QPushButton* m_sendBtn       = nullptr;
    QPushButton* m_stopBtn       = nullptr;
    QPushButton* m_keyBtn        = nullptr;

    // Control 3 — speed override slider (5..60 WPM)
    QSlider* m_speedSlider       = nullptr;
    QLabel*  m_speedValue        = nullptr;

    // Control 4 — memory buttons M1-M8 (2 rows of 4)
    QPushButton* m_memBtn[8]     = {};

    // Control 5 — repeat toggle + interval slider
    QPushButton* m_repeatBtn     = nullptr;
    QSlider*     m_repeatSlider  = nullptr;
    QLabel*      m_repeatValue   = nullptr;

    // Control 6 — keyboard-to-CW toggle
    QPushButton* m_kbCwBtn       = nullptr;

    QInputDialog* m_memoryEditor = nullptr;

    // What is being sent, in the order the sender takes it.
    QVector<Request> m_requests;
    int m_requestIndex{-1};     // the request characterStarted is in
    int m_lastIndex{-1};        // its last characterStarted index
    int m_markedCurrent{-1};    // the current character in the field
    int m_sentFrom{0};          // where the sent characters start
    int m_kbLength{0};          // KB to CW: the field length already sent
    bool m_sending{false};
    bool m_editingText{false};  // the applet itself is setting the text

    bool m_sendPermitted{true};
    QString m_sendReason;
    bool m_settingsPermitted{true};
    QString m_settingsReason;
    QString m_lastRefusal;

    // CW transmit (R-CW-49): the remote window's requests.
    RemoteSender m_remote;
    bool m_remoteKeyDown{false};
    QTimer* m_remoteKeyWatchdog = nullptr;  // the Core drops the Key 3 s after
};

} // namespace NereusSDR
