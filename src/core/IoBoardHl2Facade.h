#pragma once
// no-port-check: NereusSDR-original mirrored presentation of the Core's HL2
// I/O board state. All board logic stays in IoBoardHl2.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/IoBoardHl2Facade.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. The Core's Hermes Lite 2 I/O board
// as one read-only mirrored object, `ioBoard` (R-R3-46,
// radioHardwareVersion 3).
//
// Bound to an IoBoardHl2 (the Core) it follows the board: whether it was
// detected, its hardware version and its register mirror, which the board
// fills asynchronously from ep2 read responses after a probe.
//
// Unbound (a remote window) it holds the Core's values as they arrive and
// writes them into the window's own IoBoardHl2 (setTargetBoard), so Setup's
// HL2 I/O board tab shows the Core's board through the signals it already
// follows.
//
// Wire values: `registers` is the 256-byte register mirror as 512 upper-case
// hex digits, register 0 first. `outputs` (remote-window parity Task 14,
// radioHardwareVersion 7) is the board's output pins, one bit per output
// (o0 in bit 0), as the Core last read them back from the board's output
// register (IoBoardHl2::Register::REG_OUT_PINS, 169): the value HL2
// Options' output strip shows, as mi0bot's ucOutPinsLedStripHF shows the
// read-back of that register (setup.cs:30006-30036 [@c26a8a4]).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created (R-R3-46 fix wave). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  clearRemoteValues (follow-up). AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  `outputs` (remote-window parity Task
//                                    14, R-R3-46). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <array>

namespace NereusSDR {

class IoBoardHl2;

class IoBoardHl2Facade final : public QObject {
    Q_OBJECT
    // Reported by the Core; never written from a window.
    Q_PROPERTY(bool detected READ detected NOTIFY detectedChanged)
    Q_PROPERTY(int hardwareVersion READ hardwareVersion NOTIFY hardwareVersionChanged)
    Q_PROPERTY(QString registers READ registers NOTIFY registersChanged)
    Q_PROPERTY(int outputs READ outputs NOTIFY outputsChanged)

public:
    /// The size of IoBoardHl2's register mirror.
    static constexpr int kRegisterCount = 256;

    /// The plain reason a write to this object is refused.
    static QString readOnlyReason();

    explicit IoBoardHl2Facade(QObject* parent = nullptr);
    ~IoBoardHl2Facade() override;

    /// The Core: follow `board` (nullptr unbinds; the last values stay).
    void bindBoard(IoBoardHl2* board);
    bool isBound() const;

    /// A remote window: the board the Core's values are written into.
    void setTargetBoard(IoBoardHl2* board);

    /// A remote window: a value the Core reports. False for any other name,
    /// and always false while bound.
    bool applyRemoteProperty(const QByteArray& property, const QVariant& value);

    /// A remote window whose Core does not offer the object: forget the
    /// last Core's board (not detected, version 0, registers 0), in the
    /// held values and the target board. A no-op while bound.
    void clearRemoteValues();

    bool detected() const { return m_detected; }
    int hardwareVersion() const { return m_hardwareVersion; }
    QString registers() const;
    int outputs() const { return m_outputs; }

signals:
    void detectedChanged(bool detected);
    void hardwareVersionChanged(int version);
    void registersChanged(const QString& registers);
    void outputsChanged(int outputs);

private:
    using Registers = std::array<quint8, kRegisterCount>;

    static bool decodeRegisters(const QString& text, Registers* out);
    /// Re-read the bound board and emit each property that changed.
    void refresh();
    /// Write the held values into the target board (a remote window).
    void pushToTarget();

    QPointer<IoBoardHl2> m_board;
    QPointer<IoBoardHl2> m_target;
    QList<QMetaObject::Connection> m_boardConnections;
    bool m_detected{false};
    int m_hardwareVersion{0};
    Registers m_registers{};
    int m_outputs{0};
};

} // namespace NereusSDR
