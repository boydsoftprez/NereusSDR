// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/gui/KiwiPublicReceiverPicker.cpp [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (Longpath):
//   2026-08-23 — Portiert (Stufe 4). Begruendung der einen Abweichung
//                steht in der Kopfdatei.

#include "gui/KiwiPublicReceiverPicker.h"
#include "core/KiwiOwnSignal.h"
#include "gui/StyleConstants.h"

#include <QAccessible>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace Longpath {

namespace {

// Session-scoped cache of the last successful directory fetch, shared across
// every picker instance opened this run.  Being a process-static, it is NOT
// persisted to disk — it dies with the app, so a new session always starts
// fresh.  This is what lets repeat "Browse public…" opens re-serve the list
// without hitting the operators' servers again; "Refresh list" overwrites it.
QVector<KiwiPublicReceiver> g_sessionCache;
bool g_haveSessionCache = false;

enum Column {
    ReceiverColumn,
    LocationColumn,
    DistanceColumn,
    UsersColumn,
    ApiColumn,
    LimitsColumn,
    ColumnCount,
};

// "http://host:port" -> "host:port" (what KiwiSdrClient::normalizeEndpoint wants).
QString endpointFromUrl(const QString& url)
{
    const QUrl u(url);
    QString ep = u.host();
    if (u.port() > 0)
        ep += QStringLiteral(":") + QString::number(u.port());
    return ep.isEmpty() ? url : ep;
}
} // namespace

KiwiPublicReceiverPicker::KiwiPublicReceiverPicker(QWidget* parent)
    : QDialog(parent)
    , m_dir(new KiwiPublicDirectory(this))
{
    resize(760, 460);

    setWindowTitle(tr("Oeffentliche KiwiSDR-Empfaenger"));

    auto* outer = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Filter by name, location, or host…"));
    m_search->setClearButtonEnabled(true);
    topRow->addWidget(m_search, 1);
    m_refresh = new QPushButton(tr("Refresh list"));
    m_refresh->setToolTip(tr("Re-fetch the public directory from the network."));
    topRow->addWidget(m_refresh);
    outer->addLayout(topRow);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Receiver"), tr("Location"), tr("Entfernung"), tr("Users"),
         tr("API"), tr("Limits")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ReceiverColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(LocationColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(DistanceColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(UsersColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ApiColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(LimitsColumn, QHeaderView::ResizeToContents);
    outer->addWidget(m_table, 1);

    m_status = new QLabel(tr("Loading public receivers…"));
    m_status->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                .arg(QLatin1String(Style::kTextSecondary)));
    outer->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Add selected"));
    m_ok->setEnabled(false);
    outer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &KiwiPublicReceiverPicker::acceptCurrentRow);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_table, &QTableWidget::itemDoubleClicked, this,
            [this](QTableWidgetItem*) { acceptCurrentRow(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            [this] { m_ok->setEnabled(!m_table->selectedItems().isEmpty()); });
    connect(m_search, &QLineEdit::textChanged, this, &KiwiPublicReceiverPicker::applyFilter);
    connect(m_refresh, &QPushButton::clicked, this, &KiwiPublicReceiverPicker::startFetch);

    connect(m_dir, &KiwiPublicDirectory::ready, this, &KiwiPublicReceiverPicker::onReady);
    connect(m_dir, &KiwiPublicDirectory::failed, this, [this](const QString& err) {
        m_status->setText(tr("Could not load directory: %1").arg(err));
        m_refresh->setEnabled(true);
    });

    // Re-serve the session cache if we already fetched once this run; only the
    // first browse (or an explicit "Refresh list") touches the network.
    if (g_haveSessionCache) {
        m_fromCache = true;
        onReady(g_sessionCache);
    } else {
        startFetch();
    }
}

void KiwiPublicReceiverPicker::setPreselection(const QString& ownGrid,
                                               double frequencyHz)
{
    m_preselect = true;
    m_ownGrid = ownGrid.trimmed();
    m_frequencyHz = frequencyHz;
    applyFilter();
}

void KiwiPublicReceiverPicker::startFetch()
{
    m_fromCache = false;
    m_status->setText(tr("Loading public receivers…"));
    m_refresh->setEnabled(false);
    m_dir->fetch();
}

void KiwiPublicReceiverPicker::onReady(const QVector<KiwiPublicReceiver>& receivers)
{
    // Populate (or overwrite) the session cache from every successful fetch.
    // Serving from the cache passes the same vector straight back through here,
    // which is a harmless no-op assignment.
    g_sessionCache = receivers;
    g_haveSessionCache = true;

    m_refresh->setEnabled(true);
    m_apiReceivers.clear();
    m_hiddenWebOnly = 0;
    m_hiddenUnknown = 0;
    for (const auto& r : receivers) {
        if (r.offline) continue;
        // Honor the operator: only receivers that allow the external API are
        // listed. Web-only (ext_api == 0) are excluded entirely, as are
        // receivers that don't publish a policy (we can't confirm API is OK).
        if (!r.mayConnectViaApi()) {
            if (r.apiPolicy() == KiwiPublicReceiver::ApiPolicy::Disabled)
                ++m_hiddenWebOnly;
            else
                ++m_hiddenUnknown;
            continue;
        }
        m_apiReceivers.push_back(r);
    }
    applyFilter();
}

void KiwiPublicReceiverPicker::applyFilter()
{
    const QString needle = m_search->text().trimmed();

    // Vorauswahl (2026-11-24, "Mein Signal am Kiwi"): nach Entfernung zum
    // eigenen Locator geordnet, nur was die Frequenz abdeckt und nicht voll
    // ist. Ohne Vorauswahl bleibt es bei der Reihenfolge des Verzeichnisses.
    QVector<KiwiCandidate> ordered;
    if (m_preselect) {
        ordered = kiwiPreselect(m_apiReceivers, m_ownGrid, m_frequencyHz);
    } else {
        for (const auto& r : m_apiReceivers) {
            KiwiCandidate c;
            c.receiver = r;
            ordered.append(c);
        }
    }
    QVector<const KiwiCandidate*> matches;
    matches.reserve(ordered.size());
    for (const KiwiCandidate& c : ordered) {
        const KiwiPublicReceiver& r = c.receiver;
        if (!needle.isEmpty()
            && !r.name.contains(needle, Qt::CaseInsensitive)
            && !r.location.contains(needle, Qt::CaseInsensitive)
            && !r.url.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        matches.append(&c);
    }

    // Bug fix 2026-09-07: this used to insertRow()+setItem() one row at a
    // time, re-running on EVERY keystroke in the search box. Each
    // insertRow()/setItem() fires its own model signal, and Qt's macOS
    // accessibility bridge rebuilds the WHOLE table's accessible element
    // array from scratch on every one of those whenever an accessibility
    // observer (VoiceOver, remote-control software, or a11y-based
    // automation) is attached -- confirmed to crash Longpath the same
    // way for LogbookWindow::refreshTable() (see that fix, same date,
    // for the full writeup and Qt-source citations). The public
    // directory realistically runs several hundred entries, so this was
    // the same O((rows x cols)^2) shape, at a worse retrigger rate.
    // setRowCount() stays outside the blocked scope so the view's own
    // geometry/scrollbars stay correct; only the per-cell population is
    // batched into one accessibility notification afterward.
    m_table->setRowCount(matches.size());
    {
        const QSignalBlocker modelBlocker(m_table->model());
        for (int row = 0; row < matches.size(); ++row) {
            const KiwiPublicReceiver& r = matches.at(row)->receiver;
            const double km = matches.at(row)->km;

            auto* nameItem = new QTableWidgetItem(r.name.isEmpty() ? r.url : r.name);
            nameItem->setToolTip(r.url);
            // Carry the endpoint + a suggested name on the row's first item.
            nameItem->setData(Qt::UserRole, endpointFromUrl(r.url));
            nameItem->setData(Qt::UserRole + 1,
                              QUrl(r.url).host().left(16));  // short default name
            m_table->setItem(row, ReceiverColumn, nameItem);
            nameItem->setData(Qt::UserRole + 2, km);
            m_table->setItem(row, LocationColumn, new QTableWidgetItem(r.location));
            m_table->setItem(row, DistanceColumn, new QTableWidgetItem(
                km >= 0.0 ? QStringLiteral("%1 km").arg(qRound(km))
                          : QString()));
            m_table->setItem(row, UsersColumn, new QTableWidgetItem(
                QStringLiteral("%1/%2").arg(r.users).arg(r.usersMax)));
            m_table->setItem(row, ApiColumn, new QTableWidgetItem(r.apiBadge()));

            auto* limitsItem = new QTableWidgetItem(r.connectionLimitBadge());
            if (r.advertisesConnectionLimit()) {
                limitsItem->setToolTip(tr("This receiver advertises connection limits, "
                                          "but the public directory does not publish "
                                          "the configured duration."));
            } else {
                limitsItem->setToolTip(tr("No connection limit is advertised in the "
                                          "public directory."));
            }
            m_table->setItem(row, LimitsColumn, limitsItem);
        }
    }
    m_table->viewport()->update();
    {
        QAccessibleTableModelChangeEvent tableEvent(
            m_table, QAccessibleTableModelChangeEvent::DataChanged);
        tableEvent.setFirstRow(0);
        tableEvent.setLastRow(qMax(0, m_table->rowCount() - 1));
        tableEvent.setFirstColumn(0);
        tableEvent.setLastColumn(ColumnCount - 1);
        QAccessible::updateAccessibility(&tableEvent);
    }

    const int shown = matches.size();
    QString status = tr("%1 receivers allow API access").arg(shown);
    QStringList hiddenParts;
    if (m_hiddenWebOnly > 0)
        hiddenParts << tr("%1 web-only").arg(m_hiddenWebOnly);
    if (m_hiddenUnknown > 0)
        hiddenParts << tr("%1 policy-unknown").arg(m_hiddenUnknown);
    if (!hiddenParts.isEmpty())
        status += tr(" (%1 hidden)").arg(hiddenParts.join(QStringLiteral(", ")));
    if (m_preselect) {
        status += tr("  ·  nach Entfernung zu %1 geordnet, passend zu %2 MHz")
                      .arg(m_ownGrid.isEmpty() ? tr("(Locator fehlt)") : m_ownGrid)
                      .arg(m_frequencyHz / 1.0e6, 0, 'f', 3);
    }
    if (m_fromCache)
        status += tr("  ·  cached — use “Refresh list” to update");
    m_status->setText(status);
    m_ok->setEnabled(!m_table->selectedItems().isEmpty());
}

void KiwiPublicReceiverPicker::acceptCurrentRow()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    QTableWidgetItem* item = m_table->item(row, ReceiverColumn);
    if (!item) return;
    m_selectedEndpoint = item->data(Qt::UserRole).toString();
    m_selectedName = item->data(Qt::UserRole + 1).toString();
    m_selectedKm = item->data(Qt::UserRole + 2).toDouble();
    accept();
}

} // namespace Longpath
