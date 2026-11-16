// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 13.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original. Kernel facts carry their cites in the .cpp.
//
// A PowerMate on Linux. The kernel's own driver takes it, so hidapi never sees one:
// it reads as an input device under /dev/input. On other systems this class stays
// idle. Only the two static functions do work there, so a test on any system can run
// them.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QSocketNotifier;

namespace NereusSDR {

class NEREUS_GUI_EXPORT PowerMateLinuxInput : public QObject {   // lives on the USB worker
    Q_OBJECT

public:
    explicit PowerMateLinuxInput(QObject* parent = nullptr);
    ~PowerMateLinuxInput() override;

    void check();   // looks for the device and opens it if it's there
    void close();
    bool isOpen() const;
    // Why it's there and can't be opened, in plain words. Empty otherwise.
    const QString& failureReason() const { return m_reason; }

    // One recorded input_event: its type, code and value.
    static QList<DeviceEvent> decode(quint16 type, quint16 code, qint32 value);
    // The eventN entry under sysClassInput whose device has the right name, or empty.
    // Reads names only: it opens no device, and so works before access is granted.
    static QString findEventNode(const QString& sysClassInput);

signals:
    void deviceEvent(NereusSDR::DeviceEvent event);
    void wentAway();

private:
    void readEvents();

    int m_fd{-1};
    QSocketNotifier* m_notifier{nullptr};   // owned as a Qt child
    QString m_reason;
};

} // namespace NereusSDR
