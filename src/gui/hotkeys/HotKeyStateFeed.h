// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// What lights and displays show: which actions are on, and the window's slice. It
// follows the models' signals and never polls, and it says only what changed.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QChar>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace NereusSDR {

class SliceModel;

class HotKeyStateSource {
public:
    virtual ~HotKeyStateSource() = default;
    virtual SliceModel* windowSlice() const = 0;
    virtual bool transmitOn(const QString& id) const = 0;   // tx.mox, tx.tune, tx.twotone, tx.hold
    virtual bool muted(const SliceModel* slice) const = 0;  // a listened slice has this device's own mute
    virtual bool radioConnected() const = 0;
};

class NEREUS_GUI_EXPORT HotKeyStateFeed : public QObject {
    Q_OBJECT

public:
    struct Display {
        QString frequency;  // "14.250.000", down to the Hz
        QString mode;       // "USB"
        QString filter;     // "2.7K", "500"
        QChar slice;
        bool connected{false};  // the radio, which a TMate 2 has a light for
    };

    explicit HotKeyStateFeed(HotKeyStateSource* source, QObject* parent = nullptr);

    static QStringList actionIds();     // everything isOn can say yes to
    bool isOn(const QString& actionId) const;
    Display display() const { return m_display; }

public slots:
    // The window's slice may have changed, or a transmit state, or a listened mute.
    void refresh();

signals:
    void stateChanged(QString actionId, bool on);
    void displayChanged();

private:
    void bindSlice(SliceModel* slice);
    void update();

    HotKeyStateSource* m_source;
    QPointer<SliceModel> m_slice;
    QList<QMetaObject::Connection> m_sliceConnections;
    QHash<QString, bool> m_on;
    Display m_display;
};

} // namespace NereusSDR
