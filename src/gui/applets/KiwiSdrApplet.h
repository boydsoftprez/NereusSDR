// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/gui/KiwiSdrApplet.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Das KiwiSDR-Protokoll stammt von John Seamons (ZL/KF6VO),
// http://kiwisdr.com.
//
// Modification history (Longpath):
//   2026-08-23 — Portiert (Stufe 4: Bedienflaeche).
//
// ── Was hier von der Vorlage abweicht, und warum ────────────────────
//
// Aethers Fassung haengt an drei Bausteinen, die es bei uns nicht
// gibt: SliceColorManager, SliceLabel und ThemeManager. Alle drei
// haben bei uns eine Entsprechung, nur unter anderem Namen —
// sliceColor() aus gui/widgets/SliceColors.h, SliceModel::sliceLetter()
// und Style::role(). Die Zuordnung steht im Quelltext an jeder
// betroffenen Stelle.
//
// Zwei echte Unterschiede bleiben:
//   * Der Buchstabe einer Scheibe ist bei uns CONSTANT (sliceLetter
//     leitet sich aus sliceIndex ab). Aethers letterChanged gibt es
//     also nicht und kann auch nicht gebraucht werden.
//   * Aethers modeChanged heisst bei uns dspModeChanged und traegt
//     einen DSPMode statt einer Zeichenkette.

#pragma once

#include "core/KiwiSdrClient.h"
#include "gui/applets/AppletWidget.h"

#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QVector>

class QLabel;
class QListWidget;
class QPushButton;

namespace Longpath {

class SliceModel;
class KiwiSdrManager;

struct KiwiSdrReceiverStatus {
    QString id;
    QString name;
    KiwiSdrClient::State state{KiwiSdrClient::State::Disconnected};
    QString detail;
    QString metadataSummary;
    QString protocolSummary;
    QPointer<SliceModel> assignedSlice;
};

class KiwiSdrApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit KiwiSdrApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("KiwiSdr"); }
    QString appletTitle() const override { return QStringLiteral("KIWISDR"); }
    // Die Empfaengerliste kommt vom KiwiSdrManager, nicht aus dem
    // RadioModel — hier ist also nichts abzugleichen.
    void syncFromModel() override {}

    void setReceivers(const QVector<KiwiSdrReceiverStatus>& receivers);
    void setKiwiSdrManager(KiwiSdrManager* manager);

    // ── Mein Signal am Kiwi (2026-11-24, Entwurf C) ──────────────────
    //
    // Drei Knoepfe ueber der Empfaengerliste. Das Applet schaltet nichts
    // selbst, es meldet nur den Wunsch; der Zustand kommt vom MainWindow
    // zurueck (die Setter blockieren ihr eigenes Signal).
    void setOwnSignalActive(bool active);
    void setRecordingActive(bool active);
    // Name des Kiwi, an dem "Mein Signal" haengt; leer = noch keiner.
    void setOwnSignalReceiverName(const QString& name);

signals:
    void ownSignalToggled(bool on);
    void recordToggled(bool on);
    void otherReceiverRequested();

private:
    void rebuildReceiverList();
    QWidget* buildReceiverRow(const KiwiSdrReceiverStatus& receiver);
    QWidget* buildSliceAssignmentRow(SliceModel* slice, QWidget* parent);
    QString sliceText(SliceModel* slice) const;

    QLabel* m_emptyLabel{nullptr};
    QPushButton* m_ownSignalButton{nullptr};
    QPushButton* m_recordButton{nullptr};
    QPushButton* m_otherButton{nullptr};
    QLabel* m_ownSignalLabel{nullptr};
    QListWidget* m_receiverList{nullptr};
    QVector<KiwiSdrReceiverStatus> m_receivers;
    QList<QMetaObject::Connection> m_sliceConnections;
    QPointer<KiwiSdrManager> m_kiwiSdrManager;
};

} // namespace Longpath
