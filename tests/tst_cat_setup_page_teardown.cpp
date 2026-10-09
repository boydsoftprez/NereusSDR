// no-port-check: NereusSDR-original test that a CAT channel setup page outlives its model safely.
// 2026-10-08 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <memory>
#include <QComboBox>
#include "core/AppSettings.h"
#include "gui/setup/CatNetworkSetupPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

// MainWindow creates its RadioModel before its SetupDialog, so on quit the
// model's children (CatService among them) are destroyed while a realized
// Serial Ports or TCP/IP CAT page is still alive. CatService's teardown
// reports every channel stopped; the page must not answer by reading the
// model's slices, which the model has already deleted.
class TstCatSetupPageTeardown : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("cat-setup-teardown-%1").arg(QCoreApplication::applicationPid()));
    }
    void init() { AppSettings::instance().clear(); }

    void serialPageOutlivesModel() { outlivesModel(true); }
    void tcpPageOutlivesModel() { outlivesModel(false); }

private:
    void outlivesModel(bool serial)
    {
        auto model = std::make_unique<RadioModel>();
        model->addSlice();
        model->addSlice();
        std::unique_ptr<QWidget> page;
        if (serial) {
            page = std::make_unique<CatSerialPortsPage>(model.get());
        } else {
            page = std::make_unique<CatTcpIpPage>(model.get());
        }
        auto* primary = page->findChild<QComboBox*>(QStringLiteral("cat1Primary"));
        QVERIFY(primary);
        const int slicesShown = primary->count();
        QVERIFY(slicesShown >= 3);  // None plus the two slices
        model.reset();
        // The page stays usable and keeps what it last showed.
        QCOMPARE(primary->count(), slicesShown);
        page->show();
        page.reset();
    }
};

QTEST_MAIN(TstCatSetupPageTeardown)
#include "tst_cat_setup_page_teardown.moc"
