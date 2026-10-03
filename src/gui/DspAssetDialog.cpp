// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original bounded station DSP asset manager.

#include "DspAssetDialog.h"

#include "core/AppSettings.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/OperatorReasonText.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariantMap>
#include <QVBoxLayout>

#include <array>
#include <limits>
#include <utility>

namespace NereusSDR {

namespace {

constexpr qsizetype kMaximumListJsonBytes = 256 * 1024;
constexpr int kMaximumRows = 128;
constexpr auto kGeometrySuffix = "/Geometry";
constexpr auto kDetailsSuffix = "/DetailsExpanded";

QString humanSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024) {
        return QObject::tr("%1 MiB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 2);
    }
    if (bytes >= 1024) {
        return QObject::tr("%1 KiB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    }
    return QObject::tr("%1 bytes").arg(bytes);
}

QString shortId(const QString& id)
{
    if (id.startsWith(QStringLiteral("sha256:")) && id.size() > 23) {
        return id.left(23) + QChar(0x2026);
    }
    return id;
}

bool isExactInteger(const QVariant& value, qint64* result)
{
    qint64 converted = 0;
    switch (value.metaType().id()) {
    case QMetaType::Int: converted = value.toInt(); break;
    case QMetaType::UInt: converted = value.toUInt(); break;
    case QMetaType::LongLong: converted = value.toLongLong(); break;
    case QMetaType::ULongLong: {
        const qulonglong input = value.toULongLong();
        if (input > static_cast<qulonglong>(std::numeric_limits<qint64>::max())) {
            return false;
        }
        converted = static_cast<qint64>(input);
        break;
    }
    default: return false;
    }
    if (result) {
        *result = converted;
    }
    return true;
}

bool isExactString(const QVariant& value, QString* result)
{
    if (value.metaType().id() != QMetaType::QString) {
        return false;
    }
    if (result) {
        *result = value.toString();
    }
    return true;
}

bool isExactBool(const QVariant& value, bool* result)
{
    if (value.metaType().id() != QMetaType::Bool) {
        return false;
    }
    if (result) {
        *result = value.toBool();
    }
    return true;
}

} // namespace

DspAssetDialog::DspAssetDialog(RadioModel* radio, DspAssetKind kind, QWidget* parent)
    : DspAssetDialog(radio, radio ? radio->dspAssets() : nullptr, kind, parent)
{
}

DspAssetDialog::DspAssetDialog(RadioModel* radio, DspAssetService* service,
                               DspAssetKind kind, QWidget* parent)
    : QDialog(parent), m_radio(radio), m_service(service), m_kind(kind)
{
    setObjectName(QStringLiteral("dspAssetDialog"));
    switch (kind) {
    case DspAssetKind::NnrModel: setWindowTitle(tr("NNR Model Files")); break;
    case DspAssetKind::Ps3Correction: setWindowTitle(tr("PureSignal Correction Files")); break;
    case DspAssetKind::Nr3Model: setWindowTitle(tr("NR3 Models")); break;
    }
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumSize(560, 420);
    buildUi();
    restoreUiState();
    connect(m_details, &QGroupBox::toggled, this, [this](bool expanded) {
        m_detailsText->setVisible(expanded);
        AppSettings::instance().setValue(
            settingsPrefix() + QString::fromLatin1(kDetailsSuffix),
            expanded ? QStringLiteral("True") : QStringLiteral("False"));
        adjustSize();
    });

    if (m_service) {
        connect(m_service, &DspAssetService::requestCompleted, this,
                &DspAssetDialog::onRequestCompleted);
        connect(m_service, &DspAssetService::selectionChanged, this, [this] {
            updateSelectionSummary();
        });
        if (m_kind == DspAssetKind::Nr3Model) {
            // A remote window learns the Core's NR3 support once the session
            // is up; list the models the moment it does.
            connect(m_service, &DspAssetService::nr3SelectionChanged, this, [this] {
                const bool supported = m_service && m_service->nr3ModelsSupported();
                if (supported && m_assets.isEmpty() && m_requestId == 0) {
                    requestList();
                } else if (!supported) {
                    abortOperation(true);
                    showError(tr("This Core cannot change the NR3 model."));
                }
                updateButtons();
            });
        }
        connect(m_service, &QObject::destroyed, this, [this] {
            abortOperation(false);
            showError(tr("Model and correction files are no longer available from this Core."));
        });
    }
    if (m_radio) {
        connect(m_radio->pureSignalFacade(), &PureSignalSessionFacade::statusChanged,
                this, &DspAssetDialog::updateButtons);
        connect(m_radio, &RadioModel::stationLinkStateChanged, this,
                &DspAssetDialog::retireForSessionChange);
        connect(m_radio, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState) { retireForSessionChange(); });
    }

    if (!m_service) {
        showError(tr("Model and correction files cannot be managed on this Core."));
        updateButtons();
        return;
    }
    if (m_kind == DspAssetKind::Nr3Model && !m_service->nr3ModelsSupported()) {
        showError(tr("This Core cannot change the NR3 model."));
        updateButtons();
        return;
    }
    requestList();
}

DspAssetDialog::~DspAssetDialog()
{
    saveUiState();
    abortOperation(true);
}

void DspAssetDialog::buildUi()
{
    Style::applyDarkPageStyle(this);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    if (m_kind == DspAssetKind::NnrModel) {
        auto* selectionGroup = new QGroupBox(tr("Model selection"), this);
        selectionGroup->setObjectName(QStringLiteral("nnrAssetSelectionGroup"));
        auto* form = new QFormLayout(selectionGroup);
        const std::array<QString, 2> names{tr("Standard model"), tr("Premium model")};
        for (int slot = 0; slot < 2; ++slot) {
            auto* row = new QWidget(selectionGroup);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            m_slotSelectors[slot] = new QComboBox(row);
            m_slotSelectors[slot]->setObjectName(
                slot == 0 ? QStringLiteral("nnrStandardAssetCombo")
                          : QStringLiteral("nnrPremiumAssetCombo"));
            m_actualLabels[slot] = new QLabel(row);
            m_actualLabels[slot]->setObjectName(
                slot == 0 ? QStringLiteral("nnrStandardActualLabel")
                          : QStringLiteral("nnrPremiumActualLabel"));
            m_actualLabels[slot]->setMinimumWidth(135);
            rowLayout->addWidget(m_slotSelectors[slot], 1);
            rowLayout->addWidget(m_actualLabels[slot]);
            form->addRow(names[slot], row);
            connect(m_slotSelectors[slot], qOverload<int>(&QComboBox::activated), this,
                    [this, slot](int index) { selectNnrModel(slot, index); });
        }
        m_selectionStatus = new QLabel(selectionGroup);
        m_selectionStatus->setObjectName(QStringLiteral("nnrAssetSelectionStatus"));
        m_selectionStatus->setWordWrap(true);
        form->addRow(tr("Status"), m_selectionStatus);
        root->addWidget(selectionGroup);
    }

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("dspAssetTable"));
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {tr("Label"), tr("Format"), tr("Encoding"), tr("Size"), tr("Identity"),
         tr("Compatibility")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    root->addWidget(m_table, 1);

    m_details = new QGroupBox(tr("Details"), this);
    m_details->setObjectName(QStringLiteral("dspAssetDetails"));
    m_details->setCheckable(true);
    m_details->setChecked(false);
    auto* detailsLayout = new QVBoxLayout(m_details);
    m_detailsText = new QLabel(tr("Select a file to see its details."), m_details);
    m_detailsText->setObjectName(QStringLiteral("dspAssetDetailsText"));
    m_detailsText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailsText->setWordWrap(true);
    detailsLayout->addWidget(m_detailsText);
    root->addWidget(m_details);

    m_operationStatus = new QLabel(this);
    m_operationStatus->setObjectName(QStringLiteral("dspAssetOperationStatus"));
    m_operationStatus->setWordWrap(true);
    root->addWidget(m_operationStatus);

    auto* buttons = new QHBoxLayout();
    m_importButton = new QPushButton(tr("Import\u2026"), this);
    m_importButton->setObjectName(QStringLiteral("dspAssetImportButton"));
    m_exportButton = new QPushButton(tr("Export\u2026"), this);
    m_exportButton->setObjectName(QStringLiteral("dspAssetExportButton"));
    m_refreshButton = new QPushButton(tr("Refresh files"), this);
    m_refreshButton->setObjectName(QStringLiteral("dspAssetRefreshButton"));
    buttons->addWidget(m_importButton);
    buttons->addWidget(m_exportButton);
    buttons->addWidget(m_refreshButton);
    buttons->addStretch(1);
    if (m_kind == DspAssetKind::NnrModel) {
        m_applyButton = new QPushButton(tr("Apply models and reconnect"), this);
        m_applyButton->setObjectName(QStringLiteral("applyNnrAssetsButton"));
        buttons->addWidget(m_applyButton);
        connect(m_applyButton, &QPushButton::clicked, this, &DspAssetDialog::applyNnrModels);
    } else if (m_kind == DspAssetKind::Ps3Correction) {
        m_restoreButton = new QPushButton(tr("Restore selected"), this);
        m_restoreButton->setObjectName(QStringLiteral("restoreCorrectionAssetButton"));
        buttons->addWidget(m_restoreButton);
        connect(m_restoreButton, &QPushButton::clicked, this,
                &DspAssetDialog::restoreSelectedCorrection);
    }
    auto* closeButton = new QPushButton(tr("Close"), this);
    closeButton->setObjectName(QStringLiteral("dspAssetCloseButton"));
    buttons->addWidget(closeButton);
    root->addLayout(buttons);

    connect(m_importButton, &QPushButton::clicked, this, &DspAssetDialog::chooseImportFile);
    connect(m_exportButton, &QPushButton::clicked, this, &DspAssetDialog::chooseExportFile);
    connect(m_refreshButton, &QPushButton::clicked, this, &DspAssetDialog::requestList);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const int row = m_table->currentRow();
        if (row >= 0 && row < m_assets.size()) {
            const AssetRow& asset = m_assets.at(row);
            QString text = tr("ID: %1\nSHA-256: %2\nFormat: %3 v%4\nEncoding: %5\nSize: %6")
                               .arg(asset.id, asset.hash, asset.format)
                               .arg(m_table->item(row, 1)->data(Qt::UserRole).toInt())
                               .arg(asset.encoding, humanSize(asset.size));
            if (!asset.radioIdentity.isEmpty()) {
                text += tr("\nRadio: %1").arg(asset.radioIdentity);
            }
            if (!asset.compatibility.isEmpty()) {
                text += tr("\nCompatibility: %1").arg(asset.compatibility);
            }
            if (!asset.validationError.isEmpty()) {
                text += tr("\nValidation: %1")
                            .arg(OperatorReasonText::forDisplay(asset.validationError));
            }
            m_detailsText->setText(text);
        } else {
            m_detailsText->setText(tr("Select a file to see its details."));
        }
        updateButtons();
    });
    updateButtons();
}

QString DspAssetDialog::settingsPrefix() const
{
    switch (m_kind) {
    case DspAssetKind::NnrModel: return QStringLiteral("DspAssetDialog/NnrModel");
    case DspAssetKind::Ps3Correction: return QStringLiteral("DspAssetDialog/Ps3Correction");
    case DspAssetKind::Nr3Model: return QStringLiteral("DspAssetDialog/Nr3Model");
    }
    return QStringLiteral("DspAssetDialog/NnrModel");
}

void DspAssetDialog::restoreUiState()
{
    AppSettings& settings = AppSettings::instance();
    const QString prefix = settingsPrefix();
    const QByteArray savedGeometry = QByteArray::fromBase64(
        settings.value(prefix + QString::fromLatin1(kGeometrySuffix)).toString().toLatin1());
    bool restored = false;
    if (!savedGeometry.isEmpty()) {
        restored = restoreGeometry(savedGeometry);
    }
    if (!restored) {
        resize(760, 520);
    }

    QScreen* destination = nullptr;
    qint64 largestIntersection = 0;
    for (QScreen* screen : QGuiApplication::screens()) {
        if (!screen) {
            continue;
        }
        const QRect intersection = screen->availableGeometry().intersected(frameGeometry());
        const qint64 area = qint64(intersection.width()) * intersection.height();
        if (area > largestIntersection) {
            destination = screen;
            largestIntersection = area;
        }
    }
    if (!destination) {
        destination = parentWidget() && parentWidget()->screen()
                          ? parentWidget()->screen() : QGuiApplication::primaryScreen();
    }
    if (destination) {
        const QRect available = destination->availableGeometry();
        const int safeWidth = qMin(qMax(minimumWidth(), width()), available.width());
        const int safeHeight = qMin(qMax(minimumHeight(), height()), available.height());
        resize(safeWidth, safeHeight);
        const int x = qBound(available.left(), frameGeometry().left(),
                             qMax(available.left(), available.right() - frameGeometry().width() + 1));
        const int y = qBound(available.top(), frameGeometry().top(),
                             qMax(available.top(), available.bottom() - frameGeometry().height() + 1));
        move(x + this->geometry().left() - frameGeometry().left(),
             y + this->geometry().top() - frameGeometry().top());
    }

    const bool detailsExpanded =
        settings.value(prefix + QString::fromLatin1(kDetailsSuffix),
                       QStringLiteral("False")).toString() == QStringLiteral("True");
    {
        const QSignalBlocker blocker(m_details);
        m_details->setChecked(detailsExpanded);
    }
    m_detailsText->setVisible(detailsExpanded);
}

void DspAssetDialog::saveUiState() const
{
    AppSettings::instance().setValue(
        settingsPrefix() + QString::fromLatin1(kGeometrySuffix),
        QString::fromLatin1(saveGeometry().toBase64()));
}

qint64 DspAssetDialog::kindSizeLimit() const
{
    return DspAssetValidation::sizeLimit(m_kind);
}

QString DspAssetDialog::currentRadioIdentity() const
{
    return m_radio ? AppSettings::normalizedRadioMac(m_radio->currentRadioMac()) : QString();
}

bool DspAssetDialog::beginRequest(Operation operation, const QByteArray& verb,
                                  const QVariantMap& args)
{
    if (!m_service || m_requestId != 0) {
        return false;
    }
    m_operation = operation;
    m_requestId = m_service->request(verb, args);
    if (m_requestId == 0) {
        m_operation = Operation::Idle;
        showError(tr("This request could not be sent to the Core."));
        emit operationFinished(false, m_operationStatus->text());
        updateButtons();
        return false;
    }
    updateButtons();
    return true;
}

void DspAssetDialog::requestList()
{
    if (m_requestId != 0 || !m_service) {
        return;
    }
    m_operationStatus->setText(tr("Refreshing the Core's files\u2026"));
    beginRequest(Operation::List, "dspAssets.list", {});
}

void DspAssetDialog::handleListReply(const QVariantMap& values)
{
    QString json;
    if (!isExactString(values.value(QStringLiteral("assets")), &json)
        || json.toUtf8().size() > kMaximumListJsonBytes) {
        finishOperation(false, tr("The Core sent a file list this app could not use."));
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()
        || document.array().size() > kMaximumRows) {
        finishOperation(false, tr("The Core sent a file list this app could not use."));
        return;
    }

    QList<AssetRow> parsed;
    const QString radioIdentity = currentRadioIdentity();
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const int kindValue = object.value(QStringLiteral("kind")).toInt(-1);
        if (kindValue != static_cast<int>(m_kind)) {
            continue;
        }
        AssetRow row;
        row.kind = static_cast<DspAssetKind>(kindValue);
        row.id = object.value(QStringLiteral("id")).toString();
        row.hash = object.value(QStringLiteral("hash")).toString();
        row.label = object.value(QStringLiteral("label")).toString();
        row.format = object.value(QStringLiteral("format")).toString();
        row.encoding = object.value(QStringLiteral("numericEncoding")).toString();
        row.compatibility = object.value(QStringLiteral("compatibility")).toString();
        row.radioIdentity = object.value(QStringLiteral("radioIdentity")).toString();
        row.size = qint64(object.value(QStringLiteral("size")).toDouble(-1));
        row.valid = object.value(QStringLiteral("valid")).toBool(false);
        row.validationError = object.value(QStringLiteral("validationError")).toString();
        row.version = object.value(QStringLiteral("version")).toInt();
        if (row.id.isEmpty() || row.id.size() > 128 || row.size <= 0
            || row.size > kindSizeLimit()) {
            continue;
        }
        if (m_kind == DspAssetKind::Ps3Correction) {
            if (!row.valid) {
                continue;
            }
            if (!radioIdentity.isEmpty()
                && AppSettings::normalizedRadioMac(row.radioIdentity) != radioIdentity) {
                continue;
            }
        }
        parsed.append(std::move(row));
    }
    m_assets = std::move(parsed);

    m_table->setRowCount(m_assets.size());
    for (int rowIndex = 0; rowIndex < m_assets.size(); ++rowIndex) {
        const AssetRow& asset = m_assets.at(rowIndex);
        const QStringList text{asset.label.isEmpty() ? tr("Unnamed file") : asset.label,
                               asset.format,
                               asset.encoding,
                               humanSize(asset.size),
                               shortId(asset.id),
                               asset.valid ? asset.compatibility : asset.validationError};
        for (int column = 0; column < text.size(); ++column) {
            auto* item = new QTableWidgetItem(text.at(column));
            item->setToolTip(text.at(column));
            if (column == 1) {
                item->setData(Qt::UserRole, asset.version);
            }
            m_table->setItem(rowIndex, column, item);
        }
        m_table->item(rowIndex, 0)->setData(Qt::UserRole, asset.id);
    }
    if (!m_assets.isEmpty()) {
        m_table->selectRow(0);
    }
    populateNnrSelectors();
    finishOperation(true, tr("File list refreshed from the Core."));
}

void DspAssetDialog::populateNnrSelectors()
{
    if (m_kind != DspAssetKind::NnrModel || !m_service) {
        return;
    }
    m_populating = true;
    const std::array<QString, 2> desired = m_service->desiredNnrModelAssets();
    for (int slot = 0; slot < 2; ++slot) {
        QSignalBlocker blocker(m_slotSelectors[slot]);
        m_slotSelectors[slot]->clear();
        const QString bundled = QStringLiteral("bundled:%1").arg(slot);
        m_slotSelectors[slot]->addItem(slot == 0 ? tr("Bundled standard model")
                                                : tr("Bundled premium model"),
                                          bundled);
        for (const AssetRow& asset : std::as_const(m_assets)) {
            if (!asset.valid || asset.kind != DspAssetKind::NnrModel) {
                continue;
            }
            m_slotSelectors[slot]->addItem(
                asset.label.isEmpty() ? shortId(asset.id) : asset.label, asset.id);
        }
        int selected = m_slotSelectors[slot]->findData(desired[slot]);
        if (selected < 0) {
            m_slotSelectors[slot]->addItem(
                tr("Missing \u2014 %1").arg(shortId(desired[slot])), desired[slot]);
            selected = m_slotSelectors[slot]->count() - 1;
            m_slotSelectors[slot]->setItemData(selected,
                                               tr("The chosen file is missing on the Core or cannot be used."),
                                               Qt::ToolTipRole);
        }
        m_slotSelectors[slot]->setCurrentIndex(selected);
        m_lastAcceptedSelection[slot] = desired[slot];
    }
    m_populating = false;
    updateSelectionSummary();
}

void DspAssetDialog::updateSelectionSummary()
{
    if (m_kind != DspAssetKind::NnrModel || !m_service || !m_selectionStatus) {
        return;
    }
    const auto active = m_service->activeNnrModelAssets();
    for (int slot = 0; slot < 2; ++slot) {
        m_actualLabels[slot]->setText(tr("Active: %1").arg(shortId(active[slot])));
    }
    const QString rawStatus = m_service->nnrModelStatus();
    QString status = rawStatus.isEmpty() ? rawStatus : OperatorReasonText::forDisplay(rawStatus);
    if (m_service->nnrModelSelectionPending()) {
        status = tr("Pending reconnect. %1").arg(status);
    }
    m_selectionStatus->setText(status);
    updateButtons();
}

QString DspAssetDialog::selectedAssetId() const
{
    if (!m_table) {
        return {};
    }
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_assets.size()) {
        return {};
    }
    return m_assets.at(row).id;
}

void DspAssetDialog::updateButtons()
{
    const bool available = m_service && m_operation == Operation::Idle && m_requestId == 0
        && (m_kind != DspAssetKind::Nr3Model || m_service->nr3ModelsSupported());
    const bool hasSelection = !selectedAssetId().isEmpty();
    if (m_importButton) {
        m_importButton->setEnabled(available);
    }
    if (m_exportButton) {
        m_exportButton->setEnabled(available && hasSelection);
    }
    if (m_refreshButton) {
        m_refreshButton->setEnabled(available);
    }
    if (m_applyButton) {
        m_applyButton->setEnabled(available && m_radio);
    }
    if (m_restoreButton) {
        // R-R3-49 (parity Task 7): restoring a correction arms PureSignal
        // and keys nothing, so it follows canArm: a Core at
        // transmitSettingsVersion 7 takes it from a remote window while its
        // radio is off the air, and says why when it does not.
        const bool canRestore = m_radio && m_radio->pureSignalFacade()->canArm();
        const QString refusal = m_radio ? m_radio->pureSignalFacade()->armingRefusal() : QString();
        m_restoreButton->setEnabled(available && hasSelection && canRestore);
        m_restoreButton->setToolTip(canRestore ? QString()
            : !refusal.isEmpty() ? refusal
            : tr("Restore applies a correction and requires permission to transmit. "
                 "It is not available from a remote window; import and export work."));
    }
    for (QComboBox* selector : m_slotSelectors) {
        if (selector) {
            selector->setEnabled(available);
        }
    }
}

void DspAssetDialog::showError(const QString& error)
{
    // A Core refusal is shown in user words; the raw text is logged.
    if (m_operationStatus) {
        m_operationStatus->setText(OperatorReasonText::forDisplay(error));
    }
}

void DspAssetDialog::finishOperation(bool accepted, const QString& reason)
{
    m_requestId = 0;
    m_operation = Operation::Idle;
    if (!reason.isEmpty()) {
        // Shown in user words; operationFinished() keeps the raw reason.
        m_operationStatus->setText(OperatorReasonText::forDisplay(reason));
    }
    updateButtons();
    emit operationFinished(accepted, reason);
}

bool DspAssetDialog::importFile(const QString& path, const QString& label)
{
    if (!m_service || m_operation != Operation::Idle || m_requestId != 0) {
        return false;
    }
    auto input = std::make_unique<QFile>(path);
    if (!input->open(QIODevice::ReadOnly)) {
        showError(tr("Could not open the selected file for reading."));
        return false;
    }
    const qint64 size = input->size();
    if (size <= 0 || size > kindSizeLimit()) {
        showError(tr("The selected file is empty or exceeds the %1 format limit.")
                      .arg(humanSize(kindSizeLimit())));
        return false;
    }
    QString boundedLabel = label.trimmed();
    if (boundedLabel.isEmpty()) {
        boundedLabel = QFileInfo(path).completeBaseName().trimmed();
    }
    if (boundedLabel.isEmpty()) {
        boundedLabel = QFileInfo(path).fileName();
    }
    if (boundedLabel.size() > 128) {
        showError(tr("The file label can be at most 128 characters."));
        return false;
    }
    const QString radioIdentity = currentRadioIdentity();
    if (m_kind == DspAssetKind::Ps3Correction && radioIdentity.isEmpty()) {
        showError(tr("Connect to the correction file's radio before importing it."));
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!input->atEnd()) {
        const QByteArray chunk = input->read(DspAssetStore::kTransferChunkBytes);
        if (chunk.isEmpty() && input->error() != QFile::NoError) {
            showError(tr("Could not read the selected file."));
            return false;
        }
        hash.addData(chunk);
    }
    if (!input->seek(0)) {
        showError(tr("Could not rewind the selected file."));
        return false;
    }

    m_importFile = std::move(input);
    m_importSize = size;
    m_importOffset = 0;
    m_importLabel = boundedLabel;
    m_importHash = QString::fromLatin1(hash.result().toHex());
    m_operationStatus->setText(tr("Starting the import\u2026"));
    const QVariantMap args{{QStringLiteral("kind"), static_cast<int>(m_kind)},
                           {QStringLiteral("label"), m_importLabel},
                           {QStringLiteral("size"), m_importSize},
                           {QStringLiteral("hash"), m_importHash},
                           // Only a PureSignal correction belongs to one
                           // radio; models are refused with an identity.
                           {QStringLiteral("radioIdentity"),
                            m_kind == DspAssetKind::Ps3Correction ? radioIdentity
                                                                  : QString()}};
    if (!beginRequest(Operation::BeginImport, "dspAssets.beginImport", args)) {
        m_importFile.reset();
        return false;
    }
    return true;
}

void DspAssetDialog::requestNextImportChunk()
{
    if (!m_importFile || m_importTransferId.isEmpty()) {
        abortOperation(true);
        finishOperation(false, tr("The import was lost. Start it again."));
        return;
    }
    if (m_importOffset == m_importSize) {
        if (!beginRequest(Operation::FinishImport, "dspAssets.finishImport",
                          {{QStringLiteral("transferId"), m_importTransferId}})) {
            abortOperation(true);
        }
        return;
    }
    const QByteArray chunk = m_importFile->read(DspAssetStore::kTransferChunkBytes);
    if (chunk.isEmpty() || chunk.size() > DspAssetStore::kTransferChunkBytes
        || m_importOffset + chunk.size() > m_importSize) {
        const QString reason = tr("The file could not be read to its end. Start the import again.");
        abortOperation(true);
        finishOperation(false, reason);
        return;
    }
    m_lastImportChunkSize = chunk.size();
    m_operationStatus->setText(tr("Importing %1 of %2\u2026")
                                   .arg(humanSize(m_importOffset), humanSize(m_importSize)));
    if (!beginRequest(Operation::ImportChunk, "dspAssets.chunk",
                      {{QStringLiteral("transferId"), m_importTransferId},
                       {QStringLiteral("offset"), m_importOffset},
                       {QStringLiteral("data"), QString::fromLatin1(chunk.toBase64())}})) {
        abortOperation(true);
    }
}

bool DspAssetDialog::exportAssetToFile(const QString& assetId, const QString& path)
{
    if (!m_service || m_operation != Operation::Idle || m_requestId != 0
        || assetId.isEmpty() || assetId.size() > 128)
        return false;
    auto output = std::make_unique<QSaveFile>(path);
    if (!output->open(QIODevice::WriteOnly)) {
        showError(tr("Could not open the export destination."));
        return false;
    }
    m_exportFile = std::move(output);
    m_exportHasher = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
    m_exportAssetId = assetId;
    m_exportExpectedHash.clear();
    m_exportExpectedSize = -1;
    m_exportOffset = 0;
    m_operationStatus->setText(tr("Starting the export\u2026"));
    requestNextExportChunk();
    return m_requestId != 0;
}

void DspAssetDialog::requestNextExportChunk()
{
    if (!m_exportFile || !m_exportHasher || m_exportAssetId.isEmpty()) {
        abortOperation(false);
        finishOperation(false, tr("The export was lost. Start it again."));
        return;
    }
    if (!beginRequest(Operation::ExportChunk, "dspAssets.export",
                      {{QStringLiteral("id"), m_exportAssetId},
                       {QStringLiteral("offset"), m_exportOffset}})) {
        abortOperation(false);
    }
}

void DspAssetDialog::onRequestCompleted(quint32 id, bool accepted,
                                        const QString& reason,
                                        const QVariantMap& values)
{
    if (id == 0 || id != m_requestId || m_closing) {
        return;
    }
    const Operation completed = m_operation;
    m_requestId = 0;

    if (completed == Operation::SelectModel) {
        handleSelectionReply(accepted, reason, values);
        return;
    }
    if (!accepted) {
        const QString message = reason.isEmpty() ? tr("The Core refused this request.")
                                                  : reason;
        const bool importActive = completed == Operation::BeginImport
                                  || completed == Operation::ImportChunk
                                  || completed == Operation::FinishImport;
        abortOperation(importActive);
        finishOperation(false, message);
        return;
    }

    switch (completed) {
    case Operation::List:
        handleListReply(values);
        break;
    case Operation::BeginImport: {
        QString token;
        if (!isExactString(values.value(QStringLiteral("transferId")), &token)
            || token.isEmpty() || token.size() > 128) {
            abortOperation(false);
            finishOperation(false, tr("The Core's answer to the import could not be used. Start it again."));
            return;
        }
        m_importTransferId = token;
        requestNextImportChunk();
        break;
    }
    case Operation::ImportChunk: {
        qint64 offset = -1;
        const qint64 expected = m_importOffset + m_lastImportChunkSize;
        if (!isExactInteger(values.value(QStringLiteral("offset")), &offset)
            || offset != expected || offset > m_importSize) {
            abortOperation(true);
            finishOperation(false, tr("The import got out of step with the Core. Start it again."));
            return;
        }
        m_importOffset = offset;
        requestNextImportChunk();
        break;
    }
    case Operation::FinishImport: {
        m_importFile.reset();
        m_importTransferId.clear();
        finishOperation(true, tr("File imported and checked by the Core."));
        requestList();
        break;
    }
    case Operation::ExportChunk: {
        qint64 offset = -1;
        qint64 size = -1;
        QString hash;
        QString encoded;
        bool eof = false;
        static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-f]{64}$"));
        if (!isExactInteger(values.value(QStringLiteral("offset")), &offset)
            || !isExactInteger(values.value(QStringLiteral("size")), &size)
            || !isExactString(values.value(QStringLiteral("hash")), &hash)
            || !isExactString(values.value(QStringLiteral("data")), &encoded)
            || !isExactBool(values.value(QStringLiteral("eof")), &eof)
            || offset != m_exportOffset || size <= 0 || size > kindSizeLimit()
            || !hashPattern.match(hash).hasMatch()) {
            abortOperation(false);
            finishOperation(false, tr("The Core's answer to the export could not be used. Start it again."));
            return;
        }
        if (m_exportExpectedSize < 0) {
            m_exportExpectedSize = size;
            m_exportExpectedHash = hash;
        } else if (size != m_exportExpectedSize || hash != m_exportExpectedHash) {
            abortOperation(false);
            finishOperation(false, tr("The file changed on the Core during the export. Start it again."));
            return;
        }
        const QByteArray ascii = encoded.toLatin1();
        const auto decoded = QByteArray::fromBase64Encoding(
            ascii, QByteArray::AbortOnBase64DecodingErrors);
        if (QString::fromLatin1(ascii) != encoded || !decoded
            || decoded.decoded.toBase64() != ascii
            || decoded.decoded.size() > DspAssetStore::kTransferChunkBytes
            || m_exportOffset + decoded.decoded.size() > m_exportExpectedSize
            || (!eof && decoded.decoded.isEmpty())) {
            abortOperation(false);
            finishOperation(false, tr("The Core sent export data this app could not use. Start it again."));
            return;
        }
        if (m_exportFile->write(decoded.decoded) != decoded.decoded.size()) {
            abortOperation(false);
            finishOperation(false, tr("Writing the export destination failed."));
            return;
        }
        m_exportHasher->addData(decoded.decoded);
        m_exportOffset += decoded.decoded.size();
        if (!eof) {
            if (m_exportOffset >= m_exportExpectedSize) {
                abortOperation(false);
                finishOperation(false, tr("The export from the Core did not finish. Start it again."));
                return;
            }
            m_operationStatus->setText(tr("Exporting %1 of %2\u2026")
                                           .arg(humanSize(m_exportOffset),
                                                humanSize(m_exportExpectedSize)));
            requestNextExportChunk();
            return;
        }
        const QString actualHash = QString::fromLatin1(m_exportHasher->result().toHex());
        if (m_exportOffset != m_exportExpectedSize || actualHash != m_exportExpectedHash
            || !m_exportFile->commit()) {
            abortOperation(false);
            finishOperation(false, tr("The exported file was incomplete or did not match its checksum."));
            return;
        }
        m_exportFile.reset();
        m_exportHasher.reset();
        finishOperation(true, tr("File exported; its size and checksum match."));
        break;
    }
    case Operation::CancelImport:
        finishOperation(true, {});
        break;
    case Operation::Idle:
    case Operation::SelectModel:
        break;
    }
}

void DspAssetDialog::selectNnrModel(int slot, int comboIndex)
{
    if (m_populating || slot < 0 || slot > 1 || !m_slotSelectors[slot]
        || m_operation != Operation::Idle || m_requestId != 0)
        return;
    const QString id = m_slotSelectors[slot]->itemData(comboIndex).toString();
    if (id.isEmpty() || id == m_lastAcceptedSelection[slot]) {
        return;
    }
    m_selectingSlot = slot;
    m_operationStatus->setText(tr("Saving the model choice\u2026"));
    if (!beginRequest(Operation::SelectModel, "dspAssets.selectNnrModel",
                      {{QStringLiteral("slot"), slot}, {QStringLiteral("id"), id}})) {
        populateNnrSelectors();
        m_selectingSlot = -1;
    }
}

void DspAssetDialog::handleSelectionReply(bool accepted, const QString& reason,
                                          const QVariantMap&)
{
    if (!accepted) {
        populateNnrSelectors();
        m_selectingSlot = -1;
        finishOperation(false, reason.isEmpty() ? tr("The Core refused the model choice.")
                                                : reason);
        return;
    }
    if (m_selectingSlot >= 0 && m_selectingSlot < 2) {
        m_lastAcceptedSelection[m_selectingSlot] =
            m_slotSelectors[m_selectingSlot]->currentData().toString();
    }
    m_selectingSlot = -1;
    populateNnrSelectors();
    finishOperation(true, tr("Model choice saved. Reconnect to apply it."));
}

void DspAssetDialog::chooseImportFile()
{
    QString filter;
    switch (m_kind) {
    case DspAssetKind::NnrModel:
        filter = tr("Neural models (*.bin *.nn);;All files (*)");
        break;
    case DspAssetKind::Ps3Correction:
        filter = tr("PureSignal v2 corrections (*.txt *.ps3);;All files (*)");
        break;
    case DspAssetKind::Nr3Model:
        filter = tr("NR3 models (*.bin *.rnnn);;All files (*)");
        break;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Import file"), {}, filter);
    if (path.isEmpty()) {
        return;
    }
    bool accepted = false;
    QString label = QInputDialog::getText(this, tr("File label"), tr("Label"),
                                          QLineEdit::Normal,
                                          QFileInfo(path).completeBaseName(), &accepted);
    if (!accepted) {
        return;
    }
    if (label.size() > 128) {
        label.truncate(128);
    }
    importFile(path, label);
}

void DspAssetDialog::chooseExportFile()
{
    const QString id = selectedAssetId();
    if (id.isEmpty()) {
        return;
    }
    const QString suffix = m_kind == DspAssetKind::Ps3Correction ? QStringLiteral(".txt")
                                                                  : QStringLiteral(".bin");
    QString suggested;
    const int row = m_table->currentRow();
    if (row >= 0 && row < m_assets.size()) {
        suggested = m_assets.at(row).label;
    }
    if (suggested.isEmpty()) {
        suggested = QStringLiteral("dsp-asset");
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export file"),
                                                       suggested + suffix);
    if (!path.isEmpty()) {
        exportAssetToFile(id, path);
    }
}

void DspAssetDialog::applyNnrModels()
{
    if (m_kind != DspAssetKind::NnrModel || !m_radio || !m_service
        || m_operation != Operation::Idle)
        return;
    QString reason;
    if (!m_radio->applyNnrModelSelection(m_service->selectionRevision(), &reason)) {
        showError(reason.isEmpty() ? tr("The Core refused the reconnect request.") : reason);
        return;
    }
    m_operationStatus->setText(reason.isEmpty()
                                   ? tr("Reconnect requested. The Core reports when the models are active.")
                                   : OperatorReasonText::forDisplay(reason));
}

void DspAssetDialog::restoreSelectedCorrection()
{
    if (m_kind != DspAssetKind::Ps3Correction || m_operation != Operation::Idle
        || m_requestId != 0 || !m_radio || !m_radio->pureSignalFacade()->canArm()) {
        return;
    }
    const QString id = selectedAssetId();
    if (!id.isEmpty()) {
        emit restoreCorrectionRequested(id);
    }
}

void DspAssetDialog::abortOperation(bool requestCancellation)
{
    const QString token = m_importTransferId;
    m_requestId = 0;
    m_operation = Operation::Idle;
    if (m_importFile) {
        m_importFile->close();
    }
    m_importFile.reset();
    m_importTransferId.clear();
    m_importLabel.clear();
    m_importHash.clear();
    m_importSize = 0;
    m_importOffset = 0;
    m_lastImportChunkSize = 0;
    if (m_exportFile) {
        m_exportFile->cancelWriting();
    }
    m_exportFile.reset();
    m_exportHasher.reset();
    m_exportAssetId.clear();
    m_exportExpectedHash.clear();
    m_exportExpectedSize = -1;
    m_exportOffset = 0;
    if (requestCancellation && m_service && !token.isEmpty()) {
        // Fire and forget: this dialog has retired the transfer and deliberately
        // ignores the cancellation reply. The service owns the station request.
        m_service->request("dspAssets.cancelImport",
                           {{QStringLiteral("transferId"), token}});
    }
    updateButtons();
}

void DspAssetDialog::retireForSessionChange()
{
    if (m_closing) {
        return;
    }
    abortOperation(true);
    showError(tr("The connection to the Core changed. Reopen this window to see the Core's files again."));
    hide();
}

void DspAssetDialog::closeEvent(QCloseEvent* event)
{
    m_closing = true;
    abortOperation(true);
    saveUiState();
    m_closing = false;
    event->accept();
}

// ── Nr3ModelPicker (R-R3-21) ─────────────────────────────────────────────

Nr3ModelPicker::Nr3ModelPicker(RadioModel* radio, DspAssetService* service, QWidget* parent)
    : QWidget(parent), m_radio(radio), m_service(service)
{
    setObjectName(QStringLiteral("nr3ModelPicker"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* row = new QHBoxLayout();
    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("nr3ModelCombo"));
    m_combo->setToolTip(tr("The NR3 model the Core uses for every receiver."));
    m_modelsButton = new QPushButton(tr("Models…"), this);
    m_modelsButton->setObjectName(QStringLiteral("nr3ModelsButton"));
    m_modelsButton->setToolTip(tr("Add, save or review the Core's NR3 models."));
    row->addWidget(m_combo, 1);
    row->addWidget(m_modelsButton);
    root->addLayout(row);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("nr3ModelStatusLabel"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    connect(m_combo, qOverload<int>(&QComboBox::activated), this,
            &Nr3ModelPicker::selectModel);
    connect(m_modelsButton, &QPushButton::clicked, this, &Nr3ModelPicker::openModels);
    if (m_service) {
        connect(m_service, &DspAssetService::requestCompleted, this,
                &Nr3ModelPicker::onRequestCompleted);
        connect(m_service, &DspAssetService::nr3SelectionChanged, this, [this] {
            const bool supported = m_service && m_service->nr3ModelsSupported();
            if (supported && !m_wasSupported) {
                refresh();
            }
            if (!supported) {
                // A retired session's answers never arrive.
                m_listRequest = 0;
                m_selectRequest = 0;
            }
            m_wasSupported = supported;
            populate();
        });
    }
    m_wasSupported = m_service && m_service->nr3ModelsSupported();
    populate();
    refresh();
}

void Nr3ModelPicker::refresh()
{
    if (!m_service || !m_service->nr3ModelsSupported() || m_listRequest != 0) {
        updateState();
        return;
    }
    m_listRequest = m_service->request("dspAssets.list", {});
    if (m_listRequest == 0) {
        m_status->setText(tr("The NR3 model list could not be loaded right now."));
    }
    updateState();
}

void Nr3ModelPicker::onRequestCompleted(quint32 id, bool accepted, const QString& reason,
                                        const QVariantMap& values)
{
    if (id == 0) {
        return;
    }
    if (id == m_listRequest) {
        m_listRequest = 0;
        QString json;
        if (accepted && isExactString(values.value(QStringLiteral("assets")), &json)
            && json.toUtf8().size() <= kMaximumListJsonBytes) {
            const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
            QList<QPair<QString, QString>> models;
            if (document.isArray() && document.array().size() <= kMaximumRows) {
                for (const QJsonValue& value : document.array()) {
                    const QJsonObject object = value.toObject();
                    // Other kinds, and anything this build cannot read, are
                    // skipped rather than guessed at.
                    if (object.value(QStringLiteral("kind")).toInt(-1)
                            != static_cast<int>(DspAssetKind::Nr3Model)
                        || !object.value(QStringLiteral("valid")).toBool(false)) {
                        continue;
                    }
                    const QString assetId = object.value(QStringLiteral("id")).toString();
                    if (assetId.isEmpty() || assetId.size() > 128) {
                        continue;
                    }
                    models.append({assetId, object.value(QStringLiteral("label")).toString()});
                }
            }
            m_models = std::move(models);
        }
        populate();
        return;
    }
    if (id == m_selectRequest) {
        m_selectRequest = 0;
        m_selectFailure = accepted ? QString()
            : (reason.isEmpty() ? tr("The Core did not change the NR3 model.") : reason);
        populate();
        emit selectionFinished(accepted, m_selectFailure);
    }
}

void Nr3ModelPicker::populate()
{
    m_populating = true;
    {
        const QSignalBlocker blocker(m_combo);
        m_combo->clear();
        m_combo->addItem(tr("Bundled large model"),
                         QString::fromLatin1(DspAssetService::kNr3BundledLargeId));
        m_combo->addItem(tr("Bundled small model"),
                         QString::fromLatin1(DspAssetService::kNr3BundledSmallId));
        for (const auto& model : std::as_const(m_models)) {
            m_combo->addItem(model.second.isEmpty() ? shortId(model.first) : model.second,
                             model.first);
        }
        const QString selected = m_service ? m_service->nr3ModelAsset() : QString();
        int index = m_combo->findData(selected);
        if (index < 0 && !selected.isEmpty()) {
            m_combo->addItem(tr("Missing model"), selected);
            index = m_combo->count() - 1;
        }
        m_combo->setCurrentIndex(qMax(0, index));
    }
    m_populating = false;
    updateState();
}

void Nr3ModelPicker::updateState()
{
    const bool supported = m_service && m_service->nr3ModelsSupported();
    const bool idle = m_selectRequest == 0;
    m_combo->setEnabled(supported && idle);
    m_modelsButton->setEnabled(supported);
    if (!supported) {
        m_status->setText(tr("This Core cannot change the NR3 model."));
    } else if (!m_selectFailure.isEmpty()) {
        // Shown in user words; selectionFinished() keeps the raw reason.
        m_status->setText(OperatorReasonText::forDisplay(m_selectFailure));
    } else if (!idle) {
        m_status->setText(tr("Changing the NR3 model…"));
    } else {
        const QString status = m_service->nr3ModelStatus();
        m_status->setText(status.isEmpty() ? status : OperatorReasonText::forDisplay(status));
    }
}

void Nr3ModelPicker::selectModel(int index)
{
    if (m_populating || !m_service || !m_service->nr3ModelsSupported()
        || m_selectRequest != 0 || index < 0) {
        return;
    }
    const QString id = m_combo->itemData(index).toString();
    if (id.isEmpty() || id == m_service->nr3ModelAsset()) {
        return;
    }
    m_selectFailure.clear();
    m_selectRequest = m_service->request("dspAssets.selectNr3Model",
                                         {{QStringLiteral("id"), id}});
    if (m_selectRequest == 0) {
        m_selectFailure = tr("The NR3 model could not be changed right now.");
        populate();
        emit selectionFinished(false, m_selectFailure);
        return;
    }
    updateState();
}

void Nr3ModelPicker::openModels()
{
    if (!m_service) {
        return;
    }
    if (!m_dialog) {
        m_dialog = new DspAssetDialog(m_radio, m_service, DspAssetKind::Nr3Model, window());
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
        // An added model becomes choosable here without reopening Setup.
        connect(m_dialog, &DspAssetDialog::operationFinished, this,
                [this](bool accepted, const QString&) {
            if (accepted) {
                refresh();
            }
        });
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}

} // namespace NereusSDR
