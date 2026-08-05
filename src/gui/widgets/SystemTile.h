// no-port-check: NereusSDR-original. No upstream port. Merges the PA
// telemetry stack with the CPU metric into one two-row tile; see
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §4.3.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QWidget>

namespace NereusSDR {

class MetricLabel;

/// Two-row banner tile: PA telemetry over CPU.
///
/// The PA stack it replaces was already two rows, but the rows were
/// mutually exclusive in practice: volts fills only on MKII-class boards,
/// temperature only on HL2. CPU moves into the row that was always empty,
/// which reclaims roughly 100 px including a separator.
class SystemTile : public QWidget {
    Q_OBJECT

public:
    explicit SystemTile(QWidget* parent = nullptr);

    void setPaVolts(double volts);
    void clearPaVolts();
    void setPaTempCelsius(double celsius);
    void clearPaTemp();
    void setCpuPercent(double percent);

    /// "PA" on most boards, "PSU" on the ANAN-G2E supply_volts path.
    void setPaLabel(const QString& label);
    QString paLabel() const;

    /// Row-one value text, empty when the row is hidden.
    QString paRowText() const;
    /// Row-two value text.
    QString cpuRowText() const;
    /// False when the board publishes neither volts nor temperature.
    bool hasPaRow() const noexcept { return m_hasVolts || m_hasTemp; }

    /// Re-render row one from cached values. Call on a unit toggle.
    void refreshPaRow();

signals:
    /// Emitted on a click while row one is carrying temperature.
    void paTempClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    MetricLabel* m_paRow{nullptr};
    MetricLabel* m_cpuRow{nullptr};

    bool   m_hasVolts{false};
    bool   m_hasTemp{false};
    double m_volts{0.0};
    double m_celsius{0.0};
};

} // namespace NereusSDR
