// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/HotKeyStateFeed.h"

#include "core/NbFamily.h"
#include "core/WdspTypes.h"
#include "models/SliceModel.h"

#include <QtGlobal>

#include <cmath>
#include <cstdlib>

namespace NereusSDR {

namespace {

const QStringList& transmitIds()
{
    static const QStringList kIds = {QStringLiteral("tx.mox"), QStringLiteral("tx.tune"), QStringLiteral("tx.twotone"),
                                     QStringLiteral("tx.hold")};
    return kIds;
}

const QStringList& sliceIds()
{
    static const QStringList kIds = {QStringLiteral("mute"), QStringLiteral("slice.lock"), QStringLiteral("rit.toggle"),
                                     QStringLiteral("xit.toggle"), QStringLiteral("nr.toggle"),
                                     QStringLiteral("nb.toggle"), QStringLiteral("anf.toggle")};
    return kIds;
}

// As the on-screen controls light (ContainerButtonDispatcher::stateOf).
bool sliceOn(const QString& id, const SliceModel& slice)
{
    if (id == QLatin1String("slice.lock")) {
        return slice.locked();
    }
    if (id == QLatin1String("rit.toggle")) {
        return slice.ritEnabled();
    }
    if (id == QLatin1String("xit.toggle")) {
        return slice.xitEnabled();
    }
    if (id == QLatin1String("nr.toggle")) {
        return slice.activeNr() != NrSlot::Off;
    }
    if (id == QLatin1String("nb.toggle")) {
        return slice.nbMode() != NbMode::Off;
    }
    if (id == QLatin1String("anf.toggle")) {
        return slice.anfEnabled();
    }
    return false;
}

QString frequencyText(double hz)
{
    const qint64 whole = std::llround(hz);
    return QStringLiteral("%1.%2.%3")
        .arg(whole / 1000000)
        .arg((whole / 1000) % 1000, 3, 10, QLatin1Char('0'))
        .arg(whole % 1000, 3, 10, QLatin1Char('0'));
}

// The same words as the flag's filter label (VfoWidget::formatFilterWidth).
QString filterText(int low, int high)
{
    const int width = std::abs(high - low);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'f', 1);
    }
    return QString::number(width);
}

} // namespace

HotKeyStateFeed::HotKeyStateFeed(HotKeyStateSource* source, QObject* parent)
    : QObject(parent)
    , m_source(source)
{
    refresh();
}

QStringList HotKeyStateFeed::actionIds()
{
    return transmitIds() + sliceIds();
}

bool HotKeyStateFeed::isOn(const QString& actionId) const
{
    return m_on.value(actionId, false);
}

void HotKeyStateFeed::refresh()
{
    bindSlice(m_source->windowSlice());
    update();
}

void HotKeyStateFeed::bindSlice(SliceModel* slice)
{
    if (m_slice == slice) {
        return;
    }
    for (const QMetaObject::Connection& connection : std::as_const(m_sliceConnections)) {
        disconnect(connection);
    }
    m_sliceConnections.clear();
    m_slice = slice;
    if (slice == nullptr) {
        return;
    }
    const auto follow = [this, slice](auto signal) {
        m_sliceConnections.append(connect(slice, signal, this, &HotKeyStateFeed::update));
    };
    follow(&SliceModel::frequencyChanged);
    follow(&SliceModel::dspModeChanged);
    follow(&SliceModel::filterChanged);
    follow(&SliceModel::lockedChanged);
    follow(&SliceModel::mutedChanged);
    follow(&SliceModel::ritEnabledChanged);
    follow(&SliceModel::xitEnabledChanged);
    follow(&SliceModel::activeNrChanged);
    follow(&SliceModel::nbModeChanged);
    follow(&SliceModel::anfEnabledChanged);
    // Going away: m_slice has cleared itself by the time this runs, and the source
    // may still name the slice, so it isn't asked.
    m_sliceConnections.append(connect(slice, &QObject::destroyed, this, &HotKeyStateFeed::update, Qt::QueuedConnection));
}

void HotKeyStateFeed::update()
{
    const SliceModel* const slice = m_slice.data();
    const auto set = [this](const QString& id, bool on) {
        const auto known = m_on.constFind(id);
        const bool was = known != m_on.constEnd() && *known;
        m_on.insert(id, on);
        if (on != was) {
            emit stateChanged(id, on);
        }
    };
    for (const QString& id : transmitIds()) {
        set(id, m_source->transmitOn(id));
    }
    for (const QString& id : sliceIds()) {
        if (slice == nullptr) {
            set(id, false);
        } else if (id == QLatin1String("mute")) {
            set(id, m_source->muted(slice));
        } else {
            set(id, sliceOn(id, *slice));
        }
    }

    Display display;
    display.connected = m_source->radioConnected();
    if (slice != nullptr) {
        display.frequency = frequencyText(slice->frequency());
        display.mode = SliceModel::modeName(slice->dspMode());
        display.filter = filterText(slice->filterLow(), slice->filterHigh());
        display.slice = slice->sliceLetter();
    }
    if (display.frequency != m_display.frequency || display.mode != m_display.mode
        || display.filter != m_display.filter || display.slice != m_display.slice
        || display.connected != m_display.connected) {
        m_display = display;
        emit displayChanged();
    }
}

} // namespace NereusSDR
