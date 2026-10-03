// no-port-check: NereusSDR-original mirrored presentation of the Core's HL2
// I/O board state. All board logic stays in IoBoardHl2.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/IoBoardHl2Facade.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See IoBoardHl2Facade.h.
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

#include "core/IoBoardHl2Facade.h"

#include "core/IoBoardHl2.h"

namespace NereusSDR {

QString IoBoardHl2Facade::readOnlyReason()
{
    return QStringLiteral("The Core reports its radio's I/O board. It cannot be changed "
                          "from this app.");
}

IoBoardHl2Facade::IoBoardHl2Facade(QObject* parent)
    : QObject(parent)
{
}

IoBoardHl2Facade::~IoBoardHl2Facade() = default;

void IoBoardHl2Facade::bindBoard(IoBoardHl2* board)
{
    if (m_board == board) {
        return;
    }
    for (const QMetaObject::Connection& c : std::as_const(m_boardConnections)) {
        disconnect(c);
    }
    m_boardConnections.clear();
    m_board = board;
    if (!board) {
        return;
    }
    // The board fills its registers from the connection's ep2 read
    // responses; these arrive queued on this object's thread.
    m_boardConnections.append(
        connect(board, &IoBoardHl2::detectedChanged, this, [this](bool) { refresh(); }));
    m_boardConnections.append(
        connect(board, &IoBoardHl2::hardwareVersionChanged, this, [this](quint8) { refresh(); }));
    m_boardConnections.append(
        connect(board, &IoBoardHl2::registerChanged, this,
                [this](IoBoardHl2::Register, quint8) { refresh(); }));
    refresh();
}

bool IoBoardHl2Facade::isBound() const
{
    return !m_board.isNull();
}

void IoBoardHl2Facade::setTargetBoard(IoBoardHl2* board)
{
    m_target = board;
}

QString IoBoardHl2Facade::registers() const
{
    const QByteArray raw(reinterpret_cast<const char*>(m_registers.data()),
                         static_cast<qsizetype>(m_registers.size()));
    return QString::fromLatin1(raw.toHex()).toUpper();
}

bool IoBoardHl2Facade::decodeRegisters(const QString& text, Registers* out)
{
    if (text.size() != kRegisterCount * 2) {
        return false;
    }
    Registers parsed{};
    for (int i = 0; i < kRegisterCount; ++i) {
        bool ok = false;
        const int value = text.mid(i * 2, 2).toInt(&ok, 16);
        if (!ok) {
            return false;
        }
        parsed[static_cast<std::size_t>(i)] = static_cast<quint8>(value);
    }
    *out = parsed;
    return true;
}

bool IoBoardHl2Facade::applyRemoteProperty(const QByteArray& property, const QVariant& value)
{
    if (isBound()) {
        return false;
    }
    if (property == "detected") {
        const bool on = value.toBool();
        if (m_detected != on) {
            m_detected = on;
            emit detectedChanged(on);
        }
    } else if (property == "hardwareVersion") {
        const int version = value.toInt();
        if (m_hardwareVersion != version) {
            m_hardwareVersion = version;
            emit hardwareVersionChanged(version);
        }
    } else if (property == "outputs") {
        const int bits = value.toInt() & 0xFF;
        if (m_outputs != bits) {
            m_outputs = bits;
            emit outputsChanged(bits);
        }
    } else if (property == "registers") {
        Registers parsed{};
        if (!decodeRegisters(value.toString(), &parsed)) {
            return true;  // ours, but not a register list: keep the last one
        }
        if (m_registers != parsed) {
            m_registers = parsed;
            emit registersChanged(registers());
        }
    } else {
        return false;
    }
    pushToTarget();
    return true;
}

void IoBoardHl2Facade::clearRemoteValues()
{
    if (isBound()) {
        return;
    }
    const bool wasDetected = m_detected;
    const bool hadVersion = m_hardwareVersion != 0;
    const bool hadRegisters = m_registers != Registers{};
    const bool hadOutputs = m_outputs != 0;
    m_detected = false;
    m_hardwareVersion = 0;
    m_registers = Registers{};
    m_outputs = 0;
    if (wasDetected) { emit detectedChanged(false); }
    if (hadVersion) { emit hardwareVersionChanged(0); }
    if (hadRegisters) { emit registersChanged(registers()); }
    if (hadOutputs) { emit outputsChanged(0); }
    pushToTarget();
}

void IoBoardHl2Facade::refresh()
{
    const IoBoardHl2* board = m_board.data();
    if (!board) {
        return;
    }
    Registers next{};
    for (int i = 0; i < kRegisterCount; ++i) {
        next[static_cast<std::size_t>(i)] =
            board->registerValue(static_cast<IoBoardHl2::Register>(i));
    }
    const bool detected = board->isDetected();
    const int version = board->hardwareVersion();
    if (m_detected != detected) {
        m_detected = detected;
        emit detectedChanged(detected);
    }
    if (m_hardwareVersion != version) {
        m_hardwareVersion = version;
        emit hardwareVersionChanged(version);
    }
    if (m_registers != next) {
        m_registers = next;
        emit registersChanged(registers());
    }
    const int outputs = board->registerValue(IoBoardHl2::Register::REG_OUT_PINS);
    if (m_outputs != outputs) {
        m_outputs = outputs;
        emit outputsChanged(outputs);
    }
}

void IoBoardHl2Facade::pushToTarget()
{
    IoBoardHl2* target = m_target.data();
    if (!target) {
        return;
    }
    // Registers and version before detection, so the tab's "detected"
    // refresh reads the Core's values.
    for (int i = 0; i < kRegisterCount; ++i) {
        target->setRegisterValue(static_cast<IoBoardHl2::Register>(i),
                                 m_registers[static_cast<std::size_t>(i)]);
    }
    // The output pins last, so the strip reads the Core's `outputs` even
    // when a register list older than it arrived after it.
    target->setRegisterValue(IoBoardHl2::Register::REG_OUT_PINS,
                             static_cast<quint8>(m_outputs));
    target->setHardwareVersion(static_cast<quint8>(m_hardwareVersion));
    target->setDetected(m_detected);
}

} // namespace NereusSDR
