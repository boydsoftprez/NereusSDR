// src/gui/RelayBar.h

// =================================================================
// src/gui/RelayBar.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR, GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Extracted RelayBar widget from AetherSDR
//                 src/gui/HGauge.h (inner class) into standalone
//                 NereusSDR widget by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QWidget>

class QWheelEvent;

namespace NereusSDR {

// RelayBar: horizontal bar for relay position (0–255)
// Supports mousewheel scrolling for manual relay adjustment.
class RelayBar : public QWidget {
    Q_OBJECT

public:
    explicit RelayBar(const QString& label, QWidget* parent = nullptr);

    // Set relay position (0–255).
    void setValue(int byteValue);

    // Enable/disable mousewheel scrolling for relay adjustment.
    void setScrollEnabled(bool on);

signals:
    // Emitted on mousewheel scroll: +1 (up), -1 (down).
    void relayAdjusted(int direction);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void paintEvent(QPaintEvent*) override;

private:
    QString m_label;
    int m_value{0};
    bool m_scrollEnabled{false};
    int m_angleAccum{0};
};

} // namespace NereusSDR
