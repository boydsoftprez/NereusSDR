// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original tests for the bounded station DSP asset manager.

#include <QtTest/QtTest>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/RadioModel.h"
#include "gui/DspAssetDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>

extern "C" {
extern const unsigned char nnr_model_0_data[];
extern const unsigned int nnr_model_0_size;
}

using namespace NereusSDR;

namespace {

QByteArray standardModel()
{
    return QByteArray(reinterpret_cast<const char*>(nnr_model_0_data),
                      qsizetype(nnr_model_0_size));
}

QString writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return {};
    file.close();
    return path;
}

DspAssetServiceResult upload(DspAssetService& service, DspAssetKind kind,
                             const QByteArray& bytes, const QString& label,
                             const QString& radioIdentity = {})
{
    const QString owner = QStringLiteral("asset-dialog-fixture");
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    auto result = service.execute("dspAssets.beginImport",
                                  {{QStringLiteral("kind"), static_cast<int>(kind)},
                                   {QStringLiteral("label"), label},
                                   {QStringLiteral("size"), qint64(bytes.size())},
                                   {QStringLiteral("hash"), hash},
                                   {QStringLiteral("radioIdentity"), radioIdentity}},
                                  owner);
    if (!result.accepted) return result;
    const QString token = result.values.value(QStringLiteral("transferId")).toString();
    for (qsizetype offset = 0; offset < bytes.size();
         offset += DspAssetStore::kTransferChunkBytes) {
        const QByteArray chunk = bytes.mid(offset, DspAssetStore::kTransferChunkBytes);
        result = service.execute("dspAssets.chunk",
                                 {{QStringLiteral("transferId"), token},
                                  {QStringLiteral("offset"), qint64(offset)},
                                  {QStringLiteral("data"),
                                   QString::fromLatin1(chunk.toBase64())}},
                                 owner);
        if (!result.accepted) return result;
    }
    return service.execute("dspAssets.finishImport",
                           {{QStringLiteral("transferId"), token}}, owner);
}

bool hasSuccessfulSignal(const QSignalSpy& spy, int firstIndex)
{
    for (int index = firstIndex; index < spy.size(); ++index)
        if (spy.at(index).at(0).toBool()) return true;
    return false;
}

bool captureIfRequested(QWidget& widget, const QString& fileName)
{
    const QString directory = qEnvironmentVariable("NEREUS_DSP_UI_CAPTURE_DIR");
    if (directory.isEmpty()) {
        return true;
    }
    if (!QDir().mkpath(directory)) {
        return false;
    }
    widget.resize(qMax(widget.width(), 760), qMax(widget.height(), 520));
    widget.show();
    QApplication::processEvents();
    return widget.grab().save(QDir(directory).filePath(fileName), "PNG");
}

QByteArray bundledSmallNr3()
{
    QFile file(QFINDTESTDATA("../third_party/rnnoise/models/Default_small.bin"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList tableText(const QTableWidget* table)
{
    QStringList cells;
    for (int row = 0; row < table->rowCount(); ++row)
        for (int column = 0; column < table->columnCount(); ++column)
            cells.append(table->item(row, column) ? table->item(row, column)->text() : QString());
    return cells;
}

QStringList comboItems(const QComboBox* combo)
{
    QStringList items;
    for (int i = 0; i < combo->count(); ++i)
        items.append(combo->itemText(i) + QLatin1Char('|') + combo->itemData(i).toString());
    return items;
}

} // namespace

class TestDspAssetDialog final : public QObject
{
    Q_OBJECT

private slots:
    void importAndVerifiedExportUseBoundedServiceRequests();
    void rejectedBadFileIsNotPublished();
    void missingDesiredSelectionRemainsVisibleAndPending();
    void refusedSelectionRestoresCoreState();
    void importingDoesNotApplyOrReconnect();
    void restoreEmitsOnlySelectedCorrectionIdentity();
    void geometryAndCollapsedDetailsRoundTrip();
    void nr3DialogImportsAndListsOnlyNr3Models();
    void nnrDialogSkipsNr3RowsGolden();
    void nr3PickerLocalSelectsAndLoads();
    void nr3PickerRemoteUsesCommandsAndOlderCoreWording();
};

void TestDspAssetDialog::importAndVerifiedExportUseBoundedServiceRequests()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    DspAssetDialog dialog(nullptr, &service, DspAssetKind::NnrModel);
    QSignalSpy operations(&dialog, &DspAssetDialog::operationFinished);
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
    QVERIFY(table);
    QTRY_VERIFY(dialog.findChild<QPushButton*>(QStringLiteral("dspAssetImportButton"))
                    ->isEnabled());

    const QByteArray bytes = standardModel();
    const QString source = writeFile(directory.filePath(QStringLiteral("model.bin")), bytes);
    QVERIFY(!source.isEmpty());
    const int beforeImport = operations.size();
    QVERIFY(dialog.importFile(source, QStringLiteral("Station model")));
    QTRY_VERIFY_WITH_TIMEOUT(hasSuccessfulSignal(operations, beforeImport), 15000);
    QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 1, 15000);
    const QString id = dialog.selectedAssetId();
    QVERIFY(id.startsWith(QStringLiteral("sha256:")));
    QVERIFY2(captureIfRequested(dialog, QStringLiteral("dsp-assets-nnr.png")),
             "Could not save opt-in NNR asset manager capture");

    const QString exported = directory.filePath(QStringLiteral("roundtrip.bin"));
    const int beforeExport = operations.size();
    QVERIFY(dialog.exportAssetToFile(id, exported));
    QTRY_VERIFY_WITH_TIMEOUT(hasSuccessfulSignal(operations, beforeExport), 15000);
    QFile output(exported);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), bytes);
}

void TestDspAssetDialog::rejectedBadFileIsNotPublished()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    DspAssetDialog dialog(nullptr, &service, DspAssetKind::NnrModel);
    QSignalSpy operations(&dialog, &DspAssetDialog::operationFinished);
    QTRY_VERIFY(dialog.findChild<QPushButton*>(QStringLiteral("dspAssetImportButton"))
                    ->isEnabled());
    const QString path = writeFile(directory.filePath(QStringLiteral("bad.bin")),
                                   QByteArrayLiteral("not a WDSPNN model"));
    QVERIFY(dialog.importFile(path, QStringLiteral("Bad model")));
    QTRY_VERIFY_WITH_TIMEOUT(!operations.isEmpty()
                                 && !operations.last().at(0).toBool(), 5000);
    QCOMPARE(service.store()->assets().size(), 0);
}

void TestDspAssetDialog::missingDesiredSelectionRemainsVisibleAndPending()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    const QString missing = QStringLiteral("sha256:") + QString(64, QLatin1Char('a'));
    settings.setValue(QStringLiteral("DspAssets/NnrModel0"), missing);
    DspAssetService service(settings, true);
    DspAssetDialog dialog(nullptr, &service, DspAssetKind::NnrModel);
    auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("nnrStandardAssetCombo"));
    auto* status = dialog.findChild<QLabel*>(QStringLiteral("nnrAssetSelectionStatus"));
    QVERIFY(combo && status);
    QTRY_COMPARE(combo->currentData().toString(), missing);
    QVERIFY(combo->currentText().contains(QStringLiteral("Missing")));
    QVERIFY(service.nnrModelSelectionPending());
    QVERIFY(status->text().contains(QStringLiteral("Pending")));
    // R-R3-21: the selection rows and their status are in user words.
    int checked = 0;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text().isEmpty()) {
            continue;
        }
        QVERIFY2(OperatorWording::isPlain(label->text()), qPrintable(label->text()));
        ++checked;
    }
    // Never passes on nothing: the status, both model rows and the details.
    QVERIFY2(checked >= 4, qPrintable(QString::number(checked)));
}

void TestDspAssetDialog::refusedSelectionRestoresCoreState()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    DspAssetDialog dialog(nullptr, &service, DspAssetKind::NnrModel);
    QSignalSpy operations(&dialog, &DspAssetDialog::operationFinished);
    auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("nnrStandardAssetCombo"));
    QVERIFY(combo);
    QTRY_VERIFY(combo->isEnabled());
    const QString accepted = service.nnrStandardAsset();
    const QString unavailable = QStringLiteral("sha256:") + QString(64, QLatin1Char('f'));
    combo->addItem(QStringLiteral("Unavailable fixture"), unavailable);
    const int invalidIndex = combo->count() - 1;
    combo->setCurrentIndex(invalidIndex);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection,
                                      Q_ARG(int, invalidIndex)));
    QTRY_VERIFY_WITH_TIMEOUT(!operations.isEmpty()
                                 && !operations.last().at(0).toBool(), 5000);
    QCOMPARE(service.nnrStandardAsset(), accepted);
    QCOMPARE(combo->currentData().toString(), accepted);
}

void TestDspAssetDialog::importingDoesNotApplyOrReconnect()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    DspAssetDialog dialog(nullptr, &service, DspAssetKind::NnrModel);
    QSignalSpy operations(&dialog, &DspAssetDialog::operationFinished);
    QTRY_VERIFY(dialog.findChild<QPushButton*>(QStringLiteral("dspAssetImportButton"))
                    ->isEnabled());
    const QString path = writeFile(directory.filePath(QStringLiteral("passive.bin")),
                                   standardModel());
    const quint32 revision = service.selectionRevision();
    QVERIFY(dialog.importFile(path, QStringLiteral("Passive import")));
    QTRY_VERIFY_WITH_TIMEOUT(hasSuccessfulSignal(operations, 0), 15000);
    QCOMPARE(service.selectionRevision(), revision);
    QVERIFY(!service.nnrModelSelectionPending());
    QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("applyNnrAssetsButton"))->isEnabled());
}

void TestDspAssetDialog::restoreEmitsOnlySelectedCorrectionIdentity()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    const QString radioIdentity = QStringLiteral("AA:BB:CC:DD:EE:99");
    service.setRadioIdentity(radioIdentity);
    QFile fixture(QFINDTESTDATA("fixtures/dsp/ps3-v2-source-writer.txt"));
    QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.fileName()));
    const auto imported = upload(service, DspAssetKind::Ps3Correction, fixture.readAll(),
                                 QStringLiteral("Bench correction"), radioIdentity);
    QVERIFY2(imported.accepted, qPrintable(imported.reason));
    const QString id = imported.values.value(QStringLiteral("id")).toString();

    // Exercise the presentation contract with an explicitly authorized future
    // station; current R4 refusal is covered at the real session boundary.
    RadioModel radio(RadioModel::Role::Remote);
    radio.pureSignalFacade()->setRemoteCapabilities(true, true);
    radio.pureSignalFacade()->applyRemoteProperty("available", true);
    radio.pureSignalFacade()->applyRemoteProperty("canActuate", true);
    DspAssetDialog dialog(&radio, &service, DspAssetKind::Ps3Correction);
    QSignalSpy restored(&dialog, &DspAssetDialog::restoreCorrectionRequested);
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
    auto* button = dialog.findChild<QPushButton*>(QStringLiteral("restoreCorrectionAssetButton"));
    QVERIFY(table && button);
    QTRY_COMPARE(table->rowCount(), 1);
    table->selectRow(0);
    QTRY_VERIFY(button->isEnabled());
    QVERIFY2(captureIfRequested(dialog, QStringLiteral("dsp-assets-ps3.png")),
             "Could not save opt-in PS3 asset manager capture");
    button->click();
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first().at(0).toString(), id);
    QCOMPARE(service.store()->assets().size(), 1);
    radio.pureSignalFacade()->setRemoteCapabilities(true, false);
    QVERIFY(!button->isEnabled());
    button->click();
    QCOMPARE(restored.size(), 1);
}

void TestDspAssetDialog::geometryAndCollapsedDetailsRoundTrip()
{
    AppSettings& settings = AppSettings::instance();
    const QString geometryKey = QStringLiteral("DspAssetDialog/NnrModel/Geometry");
    const QString detailsKey = QStringLiteral("DspAssetDialog/NnrModel/DetailsExpanded");
    const bool hadGeometry = settings.contains(geometryKey);
    const bool hadDetails = settings.contains(detailsKey);
    const QVariant oldGeometry = settings.value(geometryKey);
    const QVariant oldDetails = settings.value(detailsKey);
    settings.remove(geometryKey);
    settings.remove(detailsKey);

    QSize savedSize;
    {
        DspAssetDialog dialog(nullptr, static_cast<DspAssetService*>(nullptr),
                              DspAssetKind::NnrModel);
        dialog.resize(690, 470);
        auto* details = dialog.findChild<QGroupBox*>(QStringLiteral("dspAssetDetails"));
        auto* detailsText = dialog.findChild<QLabel*>(QStringLiteral("dspAssetDetailsText"));
        QVERIFY(details && detailsText);
        QVERIFY(!details->isChecked());
        QVERIFY(detailsText->isHidden());
        details->setChecked(true);
        QVERIFY(!detailsText->isHidden());
        savedSize = dialog.size();
        dialog.close();
    }

    {
        DspAssetDialog restored(nullptr, static_cast<DspAssetService*>(nullptr),
                                DspAssetKind::NnrModel);
        auto* details = restored.findChild<QGroupBox*>(QStringLiteral("dspAssetDetails"));
        auto* detailsText = restored.findChild<QLabel*>(QStringLiteral("dspAssetDetailsText"));
        QVERIFY(details && detailsText);
        QCOMPARE(restored.size(), savedSize);
        QVERIFY(details->isChecked());
        QVERIFY(!detailsText->isHidden());
    }

    if (hadGeometry) {
        settings.setValue(geometryKey, oldGeometry);
    } else {
        settings.remove(geometryKey);
    }
    if (hadDetails) {
        settings.setValue(detailsKey, oldDetails);
    } else {
        settings.remove(detailsKey);
    }
}

void TestDspAssetDialog::nr3DialogImportsAndListsOnlyNr3Models()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    QVERIFY(upload(service, DspAssetKind::NnrModel, standardModel(),
                   QStringLiteral("NNR only")).accepted);

    DspAssetDialog dialog(nullptr, &service, DspAssetKind::Nr3Model);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("NR3 Models"));
    QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("applyNnrAssetsButton")));
    QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("restoreCorrectionAssetButton")));
    QVERIFY(!dialog.findChild<QComboBox*>(QStringLiteral("nnrStandardAssetCombo")));
    QSignalSpy operations(&dialog, &DspAssetDialog::operationFinished);
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
    QVERIFY(table);
    QTRY_VERIFY(dialog.findChild<QPushButton*>(QStringLiteral("dspAssetImportButton"))->isEnabled());
    QCOMPARE(table->rowCount(), 0);

    const QByteArray bytes = bundledSmallNr3();
    QVERIFY(!bytes.isEmpty());
    const QString source = writeFile(directory.filePath(QStringLiteral("voice.bin")), bytes);
    const int before = operations.size();
    QVERIFY(dialog.importFile(source, QStringLiteral("Voice")));
    QTRY_VERIFY_WITH_TIMEOUT(hasSuccessfulSignal(operations, before), 15000);
    QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 1, 15000);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Voice"));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("RNNoise"));
    // Importing is not choosing.
    QCOMPARE(service.nr3ModelAsset(), QString::fromLatin1(DspAssetService::kNr3BundledLargeId));
    QVERIFY2(captureIfRequested(dialog, QStringLiteral("dsp-assets-nr3.png")),
             "Could not save opt-in NR3 asset manager capture");

    const QString junk = writeFile(directory.filePath(QStringLiteral("junk.bin")),
                                   QByteArrayLiteral("this is a text file, not an NR3 model at all, really."));
    const int beforeJunk = operations.size();
    QVERIFY(dialog.importFile(junk, QStringLiteral("Junk")));
    QTRY_VERIFY_WITH_TIMEOUT(operations.size() > beforeJunk && !operations.last().at(0).toBool(), 5000);
    QCOMPARE(operations.last().at(1).toString(), QStringLiteral("This file is not an NR3 model."));
    QTRY_COMPARE(table->rowCount(), 1);
}

void TestDspAssetDialog::nnrDialogSkipsNr3RowsGolden()
{
    // A window that manages NNR models sees exactly what it saw before NR3
    // models existed, whatever NR3 rows the Core also lists.
    QTemporaryDir plainDirectory;
    AppSettings plainSettings(plainDirectory.filePath(QStringLiteral("station.settings")));
    DspAssetService plain(plainSettings, true);
    QVERIFY(upload(plain, DspAssetKind::NnrModel, standardModel(), QStringLiteral("NNR")).accepted);

    QTemporaryDir mixedDirectory;
    AppSettings mixedSettings(mixedDirectory.filePath(QStringLiteral("station.settings")));
    DspAssetService mixed(mixedSettings, true);
    QVERIFY(upload(mixed, DspAssetKind::NnrModel, standardModel(), QStringLiteral("NNR")).accepted);
    QVERIFY(upload(mixed, DspAssetKind::Nr3Model, bundledSmallNr3(), QStringLiteral("NR3")).accepted);

    DspAssetDialog golden(nullptr, &plain, DspAssetKind::NnrModel);
    DspAssetDialog withNr3(nullptr, &mixed, DspAssetKind::NnrModel);
    auto* goldenTable = golden.findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
    auto* mixedTable = withNr3.findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
    QTRY_COMPARE(goldenTable->rowCount(), 1);
    QTRY_COMPARE(mixedTable->rowCount(), 1);
    QCOMPARE(tableText(mixedTable), tableText(goldenTable));
    for (const char* name : {"nnrStandardAssetCombo", "nnrPremiumAssetCombo"}) {
        const QString objectName = QString::fromLatin1(name);
        QCOMPARE(comboItems(withNr3.findChild<QComboBox*>(objectName)),
                 comboItems(golden.findChild<QComboBox*>(objectName)));
    }
}

void TestDspAssetDialog::nr3PickerLocalSelectsAndLoads()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    QStringList loaded;
    service.setNr3ModelLoader([&](const QString& path) { loaded.append(path); });
    const auto imported = upload(service, DspAssetKind::Nr3Model, bundledSmallNr3(),
                                 QStringLiteral("Quiet"));
    QVERIFY2(imported.accepted, qPrintable(imported.reason));
    QVERIFY(upload(service, DspAssetKind::NnrModel, standardModel(), QStringLiteral("NNR")).accepted);

    Nr3ModelPicker picker(nullptr, &service);
    auto* combo = picker.findChild<QComboBox*>(QStringLiteral("nr3ModelCombo"));
    auto* models = picker.findChild<QPushButton*>(QStringLiteral("nr3ModelsButton"));
    auto* status = picker.findChild<QLabel*>(QStringLiteral("nr3ModelStatusLabel"));
    QVERIFY(combo && models && status);
    QTRY_COMPARE(combo->count(), 3); // bundled large, bundled small, "Quiet"; no NNR row
    QCOMPARE(combo->itemText(0), QStringLiteral("Bundled large model"));
    QCOMPARE(combo->itemText(1), QStringLiteral("Bundled small model"));
    QCOMPARE(combo->itemText(2), QStringLiteral("Quiet"));
    QCOMPARE(combo->currentIndex(), 0);
    QVERIFY(combo->isEnabled() && models->isEnabled());
    QCOMPARE(status->text(), QStringLiteral("Using the bundled large model."));

    QSignalSpy finished(&picker, &Nr3ModelPicker::selectionFinished);
    combo->setCurrentIndex(2);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, 2)));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().at(0).toBool());
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(service.nr3ModelAsset(), imported.values.value(QStringLiteral("id")).toString());
    QCOMPARE(status->text(), QStringLiteral("Using the NR3 model \"Quiet\"."));
    QCOMPARE(combo->currentIndex(), 2);
}

void TestDspAssetDialog::nr3PickerRemoteUsesCommandsAndOlderCoreWording()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("client.settings")));
    DspAssetService service(settings, false);
    QList<QPair<QByteArray, QVariantMap>> sent;
    quint32 nextId = 100;
    service.setRemoteRequestHandler([&](const QByteArray& verb, const QVariantMap& args) {
        sent.append({verb, args});
        return ++nextId;
    });

    // Against an older Core: disabled, says so, sends nothing.
    Nr3ModelPicker picker(nullptr, &service);
    auto* combo = picker.findChild<QComboBox*>(QStringLiteral("nr3ModelCombo"));
    auto* models = picker.findChild<QPushButton*>(QStringLiteral("nr3ModelsButton"));
    auto* status = picker.findChild<QLabel*>(QStringLiteral("nr3ModelStatusLabel"));
    QVERIFY(combo && models && status);
    QVERIFY(!combo->isEnabled());
    QVERIFY(!models->isEnabled());
    QCOMPARE(status->text(), QStringLiteral("This Core cannot change the NR3 model."));
    QVERIFY(sent.isEmpty());
    DspAssetDialog olderDialog(nullptr, &service, DspAssetKind::Nr3Model);
    QVERIFY(!olderDialog.findChild<QPushButton*>(QStringLiteral("dspAssetImportButton"))->isEnabled());
    QCOMPARE(olderDialog.findChild<QLabel*>(QStringLiteral("dspAssetOperationStatus"))->text(),
             QStringLiteral("This Core cannot change the NR3 model."));
    QVERIFY(sent.isEmpty());

    // A Core with NR3 models: the list arrives over the wire.
    service.setRemoteNr3ModelsSupported(true);
    service.applyRemoteProperty("nr3ModelStatus", QStringLiteral("Using the bundled large model."));
    QVERIFY(!sent.isEmpty());
    const int pickerList = [&] {
        for (int i = 0; i < sent.size(); ++i)
            if (sent.at(i).first == "dspAssets.list") return i;
        return -1;
    }();
    QVERIFY(pickerList >= 0);
    const QString rows = QStringLiteral(
        "[{\"id\":\"sha256:%1\",\"kind\":2,\"label\":\"Remote NR3\",\"valid\":true,\"size\":10},"
        "{\"id\":\"sha256:%2\",\"kind\":0,\"label\":\"NNR row\",\"valid\":true,\"size\":10},"
        "{\"id\":\"sha256:%3\",\"kind\":7,\"label\":\"Future row\",\"valid\":true,\"size\":10}]")
        .arg(QString(64, QLatin1Char('1')), QString(64, QLatin1Char('2')),
             QString(64, QLatin1Char('3')));
    // Answer every list request that is outstanding (the picker's and the
    // dialog's).
    for (int i = 0; i < sent.size(); ++i) {
        if (sent.at(i).first == "dspAssets.list") {
            service.receiveRemoteResult(101 + i, "dspAssets.list", true, {},
                                        {{QStringLiteral("assets"), rows}});
        }
    }
    QTRY_COMPARE(combo->count(), 3);
    QCOMPARE(combo->itemText(2), QStringLiteral("Remote NR3"));
    QVERIFY(combo->isEnabled() && models->isEnabled());
    QCOMPARE(status->text(), QStringLiteral("Using the bundled large model."));
    auto* olderTable = olderDialog.findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
    QTRY_COMPARE(olderTable->rowCount(), 1); // unknown and other kinds skipped

    const int before = sent.size();
    combo->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, 1)));
    QCOMPARE(sent.size(), before + 1);
    QCOMPARE(sent.last().first, QByteArrayLiteral("dspAssets.selectNr3Model"));
    QCOMPARE(sent.last().second, (QVariantMap{{QStringLiteral("id"),
                                  QString::fromLatin1(DspAssetService::kNr3BundledSmallId)}}));
    QVERIFY(!combo->isEnabled()); // waiting for the Core

    // The Core refuses: the reason is shown and the Core's choice stays.
    QSignalSpy finished(&picker, &Nr3ModelPicker::selectionFinished);
    QVERIFY(service.receiveRemoteResult(101 + before, "dspAssets.selectNr3Model", false,
                                        QStringLiteral("That bundled NR3 model is not installed on this Core."),
                                        {}));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(status->text(), QStringLiteral("That bundled NR3 model is not installed on this Core."));
    QCOMPARE(combo->currentIndex(), 0);
    QVERIFY(combo->isEnabled());

    // The session ends: back to the older-Core wording.
    service.setRemoteNr3ModelsSupported(false);
    QVERIFY(!combo->isEnabled());
    QCOMPARE(status->text(), QStringLiteral("This Core cannot change the NR3 model."));
}

QTEST_MAIN(TestDspAssetDialog)
#include "tst_dsp_asset_dialog.moc"
