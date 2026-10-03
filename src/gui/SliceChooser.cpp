// =================================================================
// src/gui/SliceChooser.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See SliceChooser.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: created for NereusSDR by J.J. Boyd (KG4VCF), slice control
//               and shared listening plan Task 13, with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: Take control of the Core's own
//               slice is disabled with the Core's words below
//               sliceAccessVersion 3. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: take-over review: the Core's own slice reads "the Core
//               itself", as the Core's refusal does, not "the Core's own
//               window". J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: desktop listening lane: a slice the station device holds
//               names the desktop that hosts the Core (its connectedDevices
//               entry with hostsCore), and "the Core itself" only on a Core
//               no desktop hosts; a slice the Core keeps for an away
//               device names that device. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "gui/SliceChooser.h"

#include "core/SliceOwnership.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

namespace {

// The wire id the Core sends for its own position (the station device).
const QString kStationWireId = QStringLiteral("station");

// The dashboard's own convention (RxDashboard::onFilterChanged): the whole
// passband, "2.4k" or "800".
QString filterWords(int low, int high)
{
    const int passband = (low < 0) ? (high - low) : high;
    if (passband >= 1000) {
        return QString::asprintf("%.1fk", passband / 1000.0);
    }
    return passband > 0 ? QString::number(passband) : QStringLiteral("–");
}

// "14.200.000", as the mockup and the VFO show a frequency.
QString frequencyWords(double hz)
{
    const qint64 whole = static_cast<qint64>(hz + 0.5);
    const qint64 mhz = whole / 1000000;
    const qint64 khz = (whole / 1000) % 1000;
    const qint64 rest = whole % 1000;
    return QStringLiteral("%1.%2.%3")
        .arg(mhz)
        .arg(khz, 3, 10, QLatin1Char('0'))
        .arg(rest, 3, 10, QLatin1Char('0'));
}

QString modeWords(int mode)
{
    return mode < 0 ? QStringLiteral("–") : SliceModel::modeName(static_cast<DSPMode>(mode));
}

} // namespace

SliceChooser::SliceChooser(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sliceChooser"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "#sliceChooser { background: %1; border: 1px solid %2; border-radius: 6px; }"
        "QLabel { color: %3; }"
        "QLabel[secondary=\"true\"] { color: %4; }")
        .arg(Style::kPanelBg, Style::kBorder, Style::kTextPrimary, Style::kTextSecondary));
    setMinimumWidth(300);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 8, 10, 10);
    outer->setSpacing(6);

    auto* heading = new QHBoxLayout();
    auto* title = new QLabel(tr("Slices on this Core"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    heading->addWidget(title, 1);
    auto* close = new QPushButton(QStringLiteral("×"), this);
    close->setObjectName(QStringLiteral("sliceChooserClose"));
    close->setAccessibleName(tr("Close the slice chooser"));
    close->setFlat(true);
    close->setFixedWidth(24);
    connect(close, &QPushButton::clicked, this, &SliceChooser::closeRequested);
    heading->addWidget(close);
    outer->addLayout(heading);

    m_list = new QVBoxLayout();
    m_list->setSpacing(3);
    outer->addLayout(m_list);

    m_detail = new QWidget(this);
    auto* detail = new QVBoxLayout(m_detail);
    detail->setContentsMargins(0, 4, 0, 0);
    detail->setSpacing(4);
    m_selectedLabel = new QLabel(m_detail);
    m_selectedLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_description = new QLabel(m_detail);
    m_description->setWordWrap(true);
    m_description->setProperty("secondary", true);
    m_actions = new QHBoxLayout();
    m_actions->setSpacing(6);
    detail->addWidget(m_selectedLabel);
    detail->addWidget(m_description);
    detail->addLayout(m_actions);
    outer->addWidget(m_detail);

    auto* newRow = new QHBoxLayout();
    m_newSlice = new QPushButton(tr("New slice"), this);
    m_newSlice->setObjectName(QStringLiteral("sliceChooserNewSlice"));
    m_newSlice->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(m_newSlice, &QPushButton::clicked, this, [this]() {
        if (!m_pending) {
            emit newSliceRequested();
        }
    });
    m_capacity = new QLabel(this);
    m_capacity->setWordWrap(true);
    m_capacity->setProperty("secondary", true);
    newRow->addWidget(m_newSlice);
    newRow->addWidget(m_capacity, 1);
    outer->addLayout(newRow);

    m_message = new QLabel(this);
    m_message->setObjectName(QStringLiteral("sliceChooserMessage"));
    m_message->setWordWrap(true);
    m_message->setAccessibleName(tr("Slice chooser status"));
    outer->addWidget(m_message);

    rebuild();
}

void SliceChooser::setInventory(const QList<Row>& rows)
{
    m_rows = rows;
    std::sort(m_rows.begin(), m_rows.end(),
              [](const Row& a, const Row& b) { return a.sliceId < b.sliceId; });
    const bool stillThere = std::any_of(m_rows.cbegin(), m_rows.cend(),
                                        [this](const Row& r) { return r.sliceId == m_selected; });
    if (!stillThere) {
        m_selected = -1;
        for (const Row& r : std::as_const(m_rows)) {
            if (r.activeHere) {
                m_selected = r.sliceId;
            }
        }
        if (m_selected < 0 && !m_rows.isEmpty()) {
            m_selected = m_rows.first().sliceId;
        }
    }
    rebuild();
}

void SliceChooser::setNoRoomForNewSlice(bool full)
{
    m_full = full;
    rebuild();
}

void SliceChooser::selectSlice(int sliceId)
{
    m_selected = sliceId;
    rebuild();
}

void SliceChooser::setPending(const QString& waitingWords)
{
    m_pending = true;
    m_message->setText(waitingWords);
    rebuild();
}

void SliceChooser::showResult(const QString& words)
{
    m_pending = false;
    m_message->setText(words);
    rebuild();
    emit resultShown(words);
}

VfoWidget::SliceAccess SliceChooser::flagAccessFor(const Row& row)
{
    VfoWidget::SliceAccess access;
    if (row.controller == Controller::ThisWindow) {
        access.state = VfoWidget::SliceAccess::State::Controlled;
        access.line = tr("You control");
        return access;
    }
    if (!row.listeningHere) {
        return access;
    }
    access.state = VfoWidget::SliceAccess::State::Listening;
    switch (row.controller) {
    case Controller::CoreDesktop:
        // The desktop that hosts the Core by its own name; "the Core
        // itself" only on a Core no desktop hosts.
        if (!row.controllerName.isEmpty()) {
            access.line = tr("Listening · controlled by %1").arg(row.controllerName);
            access.heldReason = tr("%1 controls this slice").arg(row.controllerName);
        } else {
            access.line = tr("Listening · controlled by the Core itself");
            access.heldReason = tr("The Core itself controls this slice");
        }
        // Core-slice take-over: Take control off with the Core's words.
        access.takeHeldReason = row.takeRefusal;
        break;
    case Controller::OtherDevice:
        access.line = tr("Listening · controlled by %1").arg(row.controllerName);
        access.heldReason = tr("%1 controls this slice").arg(row.controllerName);
        break;
    case Controller::Nobody:
    case Controller::ThisWindow:
        access.line = tr("Listening · nobody controls it");
        access.heldReason = tr("Nobody controls this slice. Take control to change it.");
        break;
    }
    return access;
}

QString SliceChooser::message() const
{
    return m_message->text();
}

void SliceChooser::beginRequest(const QByteArray& verb, const QString& waitingWords,
                                const QString& success)
{
    m_requestVerb = verb;
    m_requestSuccess = success;
    setPending(waitingWords);
}

bool SliceChooser::finishRequest(const QByteArray& verb, bool accepted, const QString& reason)
{
    if (m_requestVerb.isEmpty() || verb != m_requestVerb) {
        return false;
    }
    const QString words = accepted ? m_requestSuccess : reason;
    m_requestVerb.clear();
    m_requestSuccess.clear();
    showResult(words);
    return true;
}

void SliceChooser::linkLost()
{
    if (!m_pending && m_requestVerb.isEmpty()) {
        return;
    }
    m_requestVerb.clear();
    m_requestSuccess.clear();
    showResult(tr("The Core did not answer"));
}

void SliceChooser::reopened()
{
    if (m_pending && m_requestVerb.isEmpty()) {
        showResult(tr("The Core did not answer"));
    }
}

QString SliceChooser::ownerWords(const Row& row) const
{
    switch (row.controller) {
    case Controller::ThisWindow:  return tr("This window controls");
    case Controller::CoreDesktop:
        return row.controllerName.isEmpty() ? tr("The Core itself controls")
                                            : tr("%1 controls").arg(row.controllerName);
    case Controller::OtherDevice: return tr("%1 controls").arg(row.controllerName);
    case Controller::Nobody:      break;
    }
    return tr("Available to control");
}

QString SliceChooser::stateWords(const Row& row) const
{
    if (row.transmitting) {
        return tr("Transmitting");
    }
    if (row.controllerAway) {
        return tr("Away · reconnecting");
    }
    if (row.listeningHere) {
        return row.activeHere ? tr("Active RX here") : tr("Listening here");
    }
    return row.listenerCount == 1 ? tr("1 listening") : tr("%1 listening").arg(row.listenerCount);
}

QString SliceChooser::descriptionFor(const Row& row) const
{
    const bool mine = row.controller == Controller::ThisWindow;
    if (row.transmitting) {
        return mine ? tr("This slice is transmitting. Release is available after transmission "
                         "stops.")
                    : tr("This slice is transmitting. Take control is available after "
                         "transmission stops.");
    }
    if (mine) {
        return tr("You control tuning. Release leaves other listeners playing; with nobody "
                  "left, the slice closes.");
    }
    // Core-slice take-over: a Core that refuses the take says why.
    if (!row.takeRefusal.isEmpty()) {
        return row.listeningHere ? row.takeRefusal
                                 : tr("You can listen in. %1").arg(row.takeRefusal);
    }
    const QString who = row.controller == Controller::OtherDevice ? row.controllerName
        : row.controller == Controller::CoreDesktop
        ? (row.controllerName.isEmpty() ? tr("The Core itself") : row.controllerName)
        : QString();
    if (row.controllerAway && !who.isEmpty()) {
        return tr("%1 is away. You can listen or take control now. It loses this slice after "
                  "three minutes away.").arg(who);
    }
    if (row.listeningHere) {
        return who.isEmpty() ? tr("Nobody controls tuning. Take control to tune it.")
                             : tr("%1 controls tuning.").arg(who);
    }
    if (who.isEmpty()) {
        return tr("Nobody controls this slice. Listen in, or take control to tune it.");
    }
    return tr("Listen without changing its tuning, or take control of this same slice. %1 "
              "will keep listening.").arg(who);
}

QPushButton* SliceChooser::actionButton(const QString& words, bool primary, bool enabled)
{
    auto* b = new QPushButton(words, m_detail);
    b->setObjectName(QStringLiteral("sliceChooserAction"));
    b->setStyleSheet(primary
        ? QStringLiteral("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                         " border-radius: 3px; padding: 3px 10px; }"
                         "QPushButton:disabled { background: %4; color: %5; }")
              .arg(Style::kBlueBg, Style::kBlueText, Style::kBlueBorder, Style::kButtonBg,
                   Style::kTextInactive)
        : QString::fromLatin1(Style::kButtonStyle));
    b->setEnabled(enabled && !m_pending);
    m_actions->addWidget(b);
    return b;
}

void SliceChooser::rebuild()
{
    while (QLayoutItem* item = m_list->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            // Out of the tree now, deleted later: a button may be the one
            // whose click started this rebuild.
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }
    for (const Row& row : std::as_const(m_rows)) {
        auto* button = new QPushButton(this);
        button->setObjectName(QStringLiteral("sliceChooserRow"));
        button->setProperty("sliceId", row.sliceId);
        button->setCheckable(true);
        button->setChecked(row.sliceId == m_selected);
        button->setAccessibleName(tr("Slice %1").arg(row.letter()));
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px;"
            " text-align: left; padding: 4px; }"
            "QPushButton:checked { border: 1px solid %3; background: %4; }")
            .arg(Style::kButtonBg, Style::kBorderSubtle, Style::kAccent, Style::kButtonHover));
        auto* layout = new QHBoxLayout(button);
        layout->setContentsMargins(6, 3, 6, 3);
        layout->setSpacing(8);
        auto* tag = new QLabel(QString(row.letter()), button);
        tag->setAlignment(Qt::AlignCenter);
        tag->setFixedWidth(22);
        tag->setStyleSheet(QStringLiteral(
            "QLabel { color: #0a0a14; background: %1; border-radius: 3px;"
            " font-weight: bold; }").arg(row.color.name()));
        auto* tune = new QLabel(QStringLiteral("%1\n%2 · %3")
                                    .arg(frequencyWords(row.frequencyHz), row.mode, row.filter),
                                button);
        auto* owner = new QLabel(QStringLiteral("%1\n%2").arg(ownerWords(row), stateWords(row)),
                                 button);
        owner->setWordWrap(true);
        for (QLabel* l : {tag, tune, owner}) {
            l->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
        if (row.transmitting) {
            owner->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kRedBorder));
        }
        layout->addWidget(tag);
        layout->addWidget(tune);
        layout->addWidget(owner, 1);
        button->setMinimumHeight(tune->sizeHint().height() + 8);
        // Choosing a row only shows it.
        connect(button, &QPushButton::clicked, this, [this, id = row.sliceId]() {
            m_selected = id;
            rebuild();
        });
        m_list->addWidget(button);
    }
    if (m_rows.isEmpty()) {
        auto* empty = new QLabel(tr("No slices are running. Create a slice to start listening."),
                                 this);
        empty->setObjectName(QStringLiteral("sliceChooserEmpty"));
        empty->setWordWrap(true);
        m_list->addWidget(empty);
    }
    rebuildDetail();
    m_newSlice->setEnabled(!m_pending);
    m_capacity->setText(m_full && !m_rows.isEmpty()
                            ? tr("Existing slices can be shared without creating another.")
                            : tr("Create a slice to tune independently."));
}

void SliceChooser::rebuildDetail()
{
    while (QLayoutItem* item = m_actions->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            // Out of the tree now, deleted later: a button may be the one
            // whose click started this rebuild.
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }
    const auto found = std::find_if(m_rows.cbegin(), m_rows.cend(),
                                    [this](const Row& r) { return r.sliceId == m_selected; });
    m_detail->setVisible(found != m_rows.cend());
    if (found == m_rows.cend()) {
        return;
    }
    const Row& row = *found;
    const bool mine = row.controller == Controller::ThisWindow;
    m_selectedLabel->setText(tr("Slice %1 · %2 · %3")
                                 .arg(row.letter())
                                 .arg(frequencyWords(row.frequencyHz), row.mode));
    m_description->setText(descriptionFor(row));
    const int id = row.sliceId;
    if (!row.listeningHere) {
        connect(actionButton(tr("Listen in"), true, true), &QPushButton::clicked, this,
                [this, id]() { emit listenRequested(id); });
    } else if (!row.activeHere) {
        connect(actionButton(tr("Select RX"), true, true), &QPushButton::clicked, this,
                [this, id]() { emit selectRequested(id); });
    }
    if (!mine) {
        // Core-slice take-over: disabled with the Core's words, never
        // hidden, when the Core refuses the take.
        QPushButton* take =
            actionButton(tr("Take control"), false, !row.transmitting && row.takeRefusal.isEmpty());
        take->setToolTip(row.takeRefusal);
        connect(take, &QPushButton::clicked, this, [this, id]() { emit takeControlRequested(id); });
    }
    if (row.listeningHere) {
        if (mine) {
            connect(actionButton(tr("Release"), false, !row.transmitting), &QPushButton::clicked,
                    this, [this, id]() { emit releaseRequested(id); });
        } else {
            connect(actionButton(tr("Stop listening"), false, true), &QPushButton::clicked, this,
                    [this, id]() { emit stopListeningRequested(id); });
        }
    }
    m_actions->addStretch(1);
}

void SliceChooser::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ── Builders ─────────────────────────────────────────────────────────────

QList<SliceChooser::Row> SliceChooser::rowsForRemoteWindow(const RadioModel& model,
                                                           const SliceAccessMirror* access,
                                                           const RemoteDevicesState& devices)
{
    QList<Row> rows;
    const QString self = access ? access->selfDeviceId() : devices.selfDeviceId();
    const auto nameOf = [&devices](const QString& wireId) {
        const auto device = devices.sliceHolderDevice(wireId);
        return device ? device->name : QString();
    };
    // The station device, as a slice's holder: the desktop that hosts the
    // Core, by its connectedDevices entry with hostsCore (never by id). Empty
    // on a Core no desktop hosts, which the rows call the Core itself.
    const QString hostName = nameOf(kStationWireId);
    const auto awayOf = [&devices](const QString& wireId) {
        const auto device = devices.connectedDevice(wireId);
        return device && device->state == QLatin1String("away");
    };
    // markerOwner: the device a marker named, whose away flag the marker
    // carries while it still controls the slice.
    const auto fill = [&](Row& row, const QString& markerOwner = QString()) {
        const std::optional<SliceAccessMirror::Entry> entry =
            access ? access->entry(row.sliceId) : std::nullopt;
        if (!entry) {
            return;
        }
        const QString controller = entry->controllerDeviceId;
        row.controller = controller.isEmpty() ? Controller::Nobody
            : controller == self               ? Controller::ThisWindow
            : controller == kStationWireId     ? Controller::CoreDesktop
                                               : Controller::OtherDevice;
        // A slice the Core keeps for an away device: its marker names that
        // device, and the row names it too, never the Core.
        const bool keptForAway = !markerOwner.isEmpty() && markerOwner != kStationWireId;
        if (row.controller == Controller::CoreDesktop && keptForAway) {
            row.controller = Controller::OtherDevice;
            row.controllerName = nameOf(markerOwner);
            row.controllerAway = true;
        } else if (row.controller == Controller::CoreDesktop) {
            row.controllerName = hostName;
            row.controllerAway = false;
        } else if (row.controller == Controller::OtherDevice) {
            row.controllerName = nameOf(controller);
            row.controllerAway = awayOf(controller)
                || (row.controllerAway && controller == markerOwner);
        }
        row.listeningHere = entry->listeners.contains(self);
        row.activeHere = row.activeHere || entry->activeRx.contains(self);
        row.listenerCount = static_cast<int>(entry->listeners.size());
        row.transmitting = entry->onAir;
        // Core-slice take-over (JJ, 2026-09-30): the Core's own slice is
        // taken like any other at sliceAccessVersion 3; below it the Core
        // refuses the take, and Take control says so. A slice the Core
        // keeps for an away device (its marker names that device) may be
        // taken at any version.
        row.takeRefusal = row.controller == Controller::CoreDesktop
                && !access->coreSliceTakeable()
            ? StationClient::coreSliceTakeUnavailableReason(row.letter())
            : QString();
    };
    const SliceModel* active = model.activeSlice();
    for (SliceModel* slice : model.slices()) {
        if (slice == nullptr) {
            continue;
        }
        Row row;
        row.sliceId = slice->sliceIndex();
        row.color = VfoWidget::sliceColor(row.sliceId);
        row.frequencyHz = slice->frequency();
        row.mode = SliceModel::modeName(slice->dspMode());
        row.filter = filterWords(slice->filterLow(), slice->filterHigh());
        // A slice this window holds and no access entry names: its own.
        row.controller = Controller::ThisWindow;
        row.listeningHere = true;
        row.listenerCount = 1;
        row.activeHere = slice == active;
        fill(row);
        rows.append(row);
    }
    for (const RemoteSliceMarker& marker : devices.markers()) {
        if (model.sliceById(marker.sliceId) != nullptr) {
            continue;
        }
        Row row;
        row.sliceId = marker.sliceId;
        row.color = VfoWidget::sliceColor(row.sliceId);
        row.frequencyHz = marker.frequencyHz;
        row.mode = modeWords(marker.dspMode);
        row.filter = filterWords(marker.filterLowHz, marker.filterHighHz);
        // A marker with no ownerDeviceId is read as nobody's: the Core
        // sends ownerKind "station" both for a slice the station device
        // holds and for a slice nobody holds, so the marker alone cannot
        // tell them apart. The access entry decides (fill): a controller
        // of "station" is the Core's desktop.
        row.controller = marker.ownerDeviceId.isEmpty()      ? Controller::Nobody
            : marker.ownerDeviceId == kStationWireId         ? Controller::CoreDesktop
                                                             : Controller::OtherDevice;
        row.controllerName =
            row.controller == Controller::CoreDesktop ? hostName : marker.ownerName;
        row.controllerAway = row.controller == Controller::OtherDevice && marker.ownerAway;
        row.listenerCount = row.controller == Controller::Nobody ? 0 : 1;
        fill(row, marker.ownerDeviceId);
        if (row.controller == Controller::OtherDevice && row.controllerName.isEmpty()) {
            row.controllerName = marker.ownerName;
        }
        rows.append(row);
    }
    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.sliceId < b.sliceId; });
    return rows;
}

QList<SliceChooser::Row> SliceChooser::rowsForHostingDesktop(RadioModel& model,
                                                             StationServer& server)
{
    QList<Row> rows;
    const SliceOwnership* ownership = model.sliceOwnership();
    if (ownership == nullptr) {
        return rows;
    }
    const QByteArray self = SliceOwnership::stationDevice();
    const int active = ownership->activeFor(self);
    for (SliceModel* slice : model.slices()) {
        if (slice == nullptr) {
            continue;
        }
        const int id = slice->sliceIndex();
        const SliceOwnership::Mark mark = ownership->mark(id);
        Row row;
        row.sliceId = id;
        row.color = VfoWidget::sliceColor(id);
        row.frequencyHz = slice->frequency();
        row.mode = SliceModel::modeName(slice->dspMode());
        row.filter = filterWords(slice->filterLow(), slice->filterHigh());
        const QByteArray subject = mark.subject();
        if (subject.isEmpty()) {
            row.controller = Controller::Nobody;
        } else if (subject == self) {
            row.controller = Controller::ThisWindow;
        } else {
            row.controller = Controller::OtherDevice;
            if (const auto words = server.connectedDevices()->describe(subject)) {
                row.controllerName = words->name;
            }
            const auto entry = server.deviceSessions()->entry(subject);
            row.controllerAway = mark.isHeld()
                || !entry || entry->state == DeviceSessionRegistry::State::Away;
        }
        row.listeningHere = ownership->isListening(self, id) || subject == self;
        row.activeHere = id == active;
        row.listenerCount = static_cast<int>(ownership->listenersOf(id).size());
        row.transmitting = server.sliceOnAir(id);
        rows.append(row);
    }
    return rows;
}

QString SliceChooser::bannerState(const QList<Row>& rows)
{
    for (const Row& row : rows) {
        if (row.activeHere && row.listeningHere) {
            return row.controller == Controller::ThisWindow ? tr("You control")
                                                            : tr("Listening");
        }
    }
    return tr("Choose a slice");
}

} // namespace NereusSDR
