#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/VoiceButton.h  (NereusSDR)
// =================================================================
//
// VOICE, the third button in the TX panel's TUNE / MOX row, and the box it
// opens just above the row: M1 to M8, Repeat, and "Voice keyer panel ›".
// Design: docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (VOICE buttons) and 6.6 (Desktop VOICE, Transmit held
// elsewhere); mockup desktop-voice-button/.
//
//   Click      opens or closes the box. A message in it sends that message
//              (the radio keys by itself and returns to receive at the
//              end) and closes the box; an empty one asks for the voice
//              keyer panel with that message (panelRequested).
//   Sending    VOICE turns red, says "M2 0:01" and fills from left to
//              right; between repeats it says "M2 again in 5 s". VOICE,
//              Escape on it or MOX stops it (RecordingsAccess::stopVoice).
//              Another device's message is not this window's to stop: the
//              Core refuses, and the refusal is shown.
//   Held       another device holds transmit (setTransmitPermitted false):
//              VOICE dims, its messages are disabled with the reason, and
//              the box offers Take transmit, as TUNE and MOX are gated.
//   Older Core VOICE stays, disabled, with the access's reason (V9).
//   Switched  Setup > Recordings' "Show the VOICE button beside TUNE and
//   off       MOX" (this computer's VoiceButtonShown, on by default) off:
//              VOICE leaves the row and TUNE and MOX close up. The voice
//              keyer panel and the hot keys still send messages.
//
// One per TxKeyingSection, owned by it. Reads the `voiceKeyer` part of the
// recordings document; keys nothing but through sendVoice().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
//   2026-11-17 - J.J. Boyd (KG4VCF), JJ's request: a switch in Setup takes
//                 VOICE out of the row (VoiceButtonShown). AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QFrame>
#include <QIcon>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <functional>
#include <optional>

#include "gui/NereusGuiExport.h"

class QLabel;
class QPushButton;
class QTimer;

namespace NereusSDR {

class RecordingsAccess;
class TxKeyingSection;

/// The `voiceKeyer` part of the recordings document, read once.
struct VoiceKeyerSnapshot {
    struct Slot {
        int n = 0;
        QString name;
        qint64 lengthMs = 0;
        bool empty = true;
    };
    struct Sending {
        int n = 0;
        qint64 positionMs = 0;
        qint64 lengthMs = 0;
        qint64 waitingMs = 0;
        QString by;
        QString deviceId;
    };
    std::array<Slot, 8> messages{};
    bool repeat = false;
    int repeatDelaySec = 5;
    std::optional<Sending> sending;

    static VoiceKeyerSnapshot from(const QJsonObject& recordingsDocument);
    /// What a message is called: its name, or "M<n>" without one.
    QString label(int n) const;
    /// Mn is going out, or waiting to repeat (clear and import refused).
    bool onAir(int n) const { return sending.has_value() && sending->n == n; }
    /// "M<n> is on the air.", as the Core refuses it.
    static QString onAirReason(int n) { return QStringLiteral("M%1 is on the air.").arg(n); }
    /// "0:06": minutes and seconds.
    static QString clockText(qint64 ms);
};

class NEREUS_GUI_EXPORT VoiceButton : public QObject {
    Q_OBJECT

public:
    /// recordingsVersion the voice keyer needs (V9 below it).
    static constexpr int kNeedsVersion = 3;
    /// How often the running time and the fill are redrawn while sending.
    static constexpr int kRefreshMs = 100;

    VoiceButton(TxKeyingSection* section, RecordingsAccess* access);
    ~VoiceButton() override;

    /// This computer's AppSettings key ("True" / "False", on when unset)
    /// for VOICE in the TUNE / MOX row. Every VOICE follows it as it is
    /// written, through the row's SectionParts "voice" part.
    static QString shownSettingKey();
    static bool shownSetting();
    static void setShownSetting(bool shown);

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const;

    /// As TUNE and MOX are gated (TxApplet::setTransmitPermitted): while
    /// not permitted, VOICE dims and its messages are disabled with
    /// `reason`.
    void setTransmitPermitted(bool permitted, const QString& reason);
    /// The box offers Take transmit (TxApplet::setTakeTransmitOffered).
    void setTakeTransmitOffered(bool offered);

    QPushButton* button() const;
    bool boxOpen() const;
    void openBox();
    void closeBox();

    // ---- Test seams.
    QFrame* boxForTest() const { return m_box; }
    QPushButton* tileForTest(int n) const;
    QPushButton* repeatForTest() const { return m_repeat; }
    QPushButton* panelLinkForTest() const { return m_panelLink; }
    QPushButton* takeTransmitForTest() const { return m_take; }
    QLabel* holderNoteForTest() const { return m_holderNote; }
    QLabel* noteForTest() const { return m_note; }
    /// The fill VOICE shows while sending, 0 to 1.
    double fillForTest() const { return m_fill; }
    /// Milliseconds since any start; the running time counts on from the
    /// document's last position with it.
    void setClockForTest(std::function<qint64()> now) { m_now = std::move(now); }
    void refreshForTest() { refresh(); }

signals:
    /// "Voice keyer panel ›" (n 0) or an empty message (n 1 to 8).
    void panelRequested(int n);
    void takeTransmitRequested();
    /// The Core refused what this button asked, with its words.
    void refusalShown(const QString& reason);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildBox();
    void onDocumentChanged();
    void refresh();
    void refreshBox();
    void onVoiceClicked();
    void onTileClicked(int n);
    void stopSending();
    void positionBox();
    QString unavailableReason() const;
    qint64 now() const;
    void applyShownSetting();

    QPointer<TxKeyingSection> m_section;
    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    VoiceKeyerSnapshot m_snapshot;
    qint64 m_snapshotAtMs = 0;
    bool m_permitted = true;
    QString m_permittedReason;
    bool m_takeOffered = false;
    bool m_awaitingAnswer = false;
    QString m_lastRefusal;
    double m_fill = 0.0;
    QString m_lastStyle;
    QIcon m_icon;

    QTimer* m_timer = nullptr;
    QFrame* m_box = nullptr;
    std::array<QPushButton*, 8> m_tiles{};
    std::array<QLabel*, 8> m_tileNames{};
    std::array<QLabel*, 8> m_tileLengths{};
    std::array<QFrame*, 8> m_tileProgress{};
    QPushButton* m_panelLink = nullptr;
    QPushButton* m_repeat = nullptr;
    QLabel* m_repeatSub = nullptr;
    QFrame* m_holderFrame = nullptr;
    QLabel* m_holderNote = nullptr;
    QPushButton* m_take = nullptr;
    QLabel* m_note = nullptr;

    std::function<qint64()> m_now;
    // AppSettings::addWriteObserver's id for VoiceButtonShown, or -1.
    int m_settingsObserver = -1;
};

}  // namespace NereusSDR
