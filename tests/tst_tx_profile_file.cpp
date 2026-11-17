// no-port-check: NereusSDR-original.
// One transmit profile file, and import into the per-radio bank.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "core/TxProfileFile.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("aa:bb:cc:11:22:33");

QString field(const QString& name, const QString& key)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(kMac, name, key);
}
} // namespace

class TstTxProfileFile : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    void roundTripKeepsTheNameAndKeys()
    {
        TxProfileFile::Document in;
        in.name = QStringLiteral("DX");
        in.keys.insert(QStringLiteral("MicGain"), QStringLiteral("-12"));
        in.keys.insert(QStringLiteral("FilterLow"), QStringLiteral("150"));
        QByteArray bytes;
        QString error;
        QVERIFY(TxProfileFile::encode(in, &bytes, &error));
        QVERIFY(error.isEmpty());
        TxProfileFile::Document out;
        QVERIFY(TxProfileFile::decode(bytes, &out, &error));
        QCOMPARE(out.name, QStringLiteral("DX"));
        QCOMPARE(out.keys.value(QStringLiteral("MicGain")), QStringLiteral("-12"));
        QCOMPARE(out.keys.value(QStringLiteral("FilterLow")), QStringLiteral("150"));
    }

    void aNewerFileIsRefused()
    {
        const QByteArray bytes = QByteArrayLiteral(
            "{\"nereusTxProfileVersion\":2,\"name\":\"DX\",\"keys\":{}}");
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(bytes, &out, &error));
        QVERIFY(error.contains(QStringLiteral("newer NereusSDR")));
    }

    void aFileThatIsNotAProfileIsRefused()
    {
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(QByteArrayLiteral("[]"), &out, &error));
        QCOMPARE(error, QStringLiteral("This file is not a transmit profile."));
        QVERIFY(!TxProfileFile::decode(QByteArray(TxProfileFile::kMaxBytes + 1, 'x'), &out, &error));
        QCOMPARE(error, QStringLiteral("The transmit profile is too large."));
    }

    void aSettingThatIsNotTextIsRefused()
    {
        const QByteArray bytes = QByteArrayLiteral(
            "{\"nereusTxProfileVersion\":1,\"name\":\"DX\",\"keys\":{\"MicGain\":-12}}");
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(bytes, &out, &error));
        QCOMPARE(error, QStringLiteral("A setting in the transmit profile is not text."));
    }

    void importFillsMissingKeysSelectsAndDropsUnknownOnes()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel tx;
        tx.setMicGainDb(-3);
        QHash<QString, QVariant> values;
        values.insert(QStringLiteral("MicGain"), QStringLiteral("-18"));
        values.insert(QStringLiteral("NotAProfileKey"), QStringLiteral("nope"));
        QVERIFY(mgr.importProfile(QStringLiteral(" Desk,Night "), values, &tx));
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Desk_Night"));
        QCOMPARE(tx.micGainDb(), -18);
        QCOMPARE(AppSettings::instance().value(field(QStringLiteral("Desk_Night"),
                                                     QStringLiteral("MicGain"))).toString(),
                 QStringLiteral("-18"));
        QCOMPARE(AppSettings::instance().value(field(QStringLiteral("Desk_Night"),
                                                     QStringLiteral("FilterLow"))).toString(),
                 MicProfileManager::defaultProfileValues()
                     .value(QStringLiteral("FilterLow")).toString());
        QVERIFY(!AppSettings::instance().contains(
            field(QStringLiteral("Desk_Night"), QStringLiteral("NotAProfileKey"))));
    }

    void importWithoutARadioDoesNothing()
    {
        MicProfileManager mgr;
        TransmitModel tx;
        QVERIFY(!mgr.importProfile(QStringLiteral("Desk"), {}, &tx));
        QVERIFY(mgr.profileNames().isEmpty());
    }
};

QTEST_MAIN(TstTxProfileFile)
#include "tst_tx_profile_file.moc"
