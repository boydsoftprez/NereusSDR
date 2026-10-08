// =================================================================
// tests/tst_app_icon.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Radio speaker and Audio Setup plan, Task 5 (R-SPK-19, D7): the app's own
// icon set replaces colour emoji text on the lock, pin and feature buttons.
//
// Coverage:
//   1. Every icon renders at 16, 18 and 22 px, at dpr 1 and 2, to the
//      expected device size with visible pixels.
//   2. Rendering is untinted: pc-on's cyan wave stays cyan.
//   3. An unknown name warns and returns a null pixmap and icon.
//   4. Renders are cached by name, size and dpr.
//   5. The resource SVGs are byte-identical to the design folder's.
//   6. The RX applet lock, the VFO flag lock, the container pin and the
//      feature button show the expected icon names.
//   7. The replaced emoji code points are gone from those source files.
//   8. With NEREUS_ICON_CAPTURE_DIR set, captures of each site are saved
//      (run once plain and once with QT_SCALE_FACTOR=2 for 1x and 2x).
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 5.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================
#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QPushButton>
#include <QRegularExpression>

#include "core/AudioEngine.h"
#include "gui/TitleBar.h"
#include "gui/applets/RxApplet.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QStringList kNames{
    QStringLiteral("pc-on"),     QStringLiteral("pc-muted"),
    QStringLiteral("radio-on"),  QStringLiteral("radio-muted"),
    QStringLiteral("radio-none"), QStringLiteral("lock"),
    QStringLiteral("unlock"),    QStringLiteral("bulb"),
    QStringLiteral("pin"),       QStringLiteral("pinned"),
};

QString sourcePath(const QString& rel)
{
    return QStringLiteral(NEREUS_SOURCE_DIR) + QLatin1Char('/') + rel;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

bool hasVisiblePixel(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                return true;
            }
        }
    }
    return false;
}

QString iconName(const QWidget* w)
{
    return w ? w->property(AppIcon::kIconProperty).toString() : QString();
}

void capture(QWidget* w, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_ICON_CAPTURE_DIR");
    if (dir.isEmpty() || !w) {
        return;
    }
    QDir().mkpath(dir);
    const QPixmap shot = w->grab();
    const int scale = qRound(shot.devicePixelRatio());
    const QString path = QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(scale);
    QVERIFY2(shot.save(path), qPrintable(path));
}

} // namespace

class TstAppIcon : public QObject {
    Q_OBJECT

private slots:

    void every_icon_renders_at_each_size_and_dpr()
    {
        for (const QString& name : kNames) {
            for (const int px : {16, 18, 22}) {
                for (const qreal dpr : {1.0, 2.0}) {
                    const QPixmap pm = AppIcon::pixmap(name, px, dpr);
                    const QByteArray what = QStringLiteral("%1 %2px @%3")
                                                .arg(name).arg(px).arg(dpr).toUtf8();
                    QVERIFY2(!pm.isNull(), what.constData());
                    QCOMPARE(pm.width(), qRound(px * dpr));
                    QCOMPARE(pm.height(), qRound(px * dpr));
                    QCOMPARE(pm.devicePixelRatio(), dpr);
                    QVERIFY2(hasVisiblePixel(pm.toImage()), what.constData());
                }
            }
            QVERIFY(!AppIcon::icon(name, 16).isNull());
        }
    }

    void render_is_untinted()
    {
        // The outer wave of pc-on crosses x 54, y 32 of its 64 x 64 view
        // box and is stroked with a cyan gradient. Untinted, some pixel near
        // there is clearly cyan: strong green and blue, little red.
        const QImage image = AppIcon::pixmap(QStringLiteral("pc-on"), 64, 1.0).toImage();
        bool cyan = false;
        for (int y = 28; y <= 36 && !cyan; ++y) {
            for (int x = 51; x <= 58 && !cyan; ++x) {
                const QColor c = image.pixelColor(x, y);
                if (c.alpha() > 200 && c.blue() > 150 && c.green() > 120 && c.red() < 120) {
                    cyan = true;
                }
            }
        }
        QVERIFY(cyan);
    }

    void unknown_name_warns_and_returns_null()
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("no icon named")));
        QVERIFY(AppIcon::pixmap(QStringLiteral("no-such-icon"), 16, 1.0).isNull());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("no icon named")));
        QVERIFY(AppIcon::icon(QStringLiteral("no-such-icon-either"), 16).isNull());
    }

    void renders_are_cached()
    {
        const QPixmap a = AppIcon::pixmap(QStringLiteral("lock"), 18, 2.0);
        const QPixmap b = AppIcon::pixmap(QStringLiteral("lock"), 18, 2.0);
        QCOMPARE(a.cacheKey(), b.cacheKey());
        const QPixmap c = AppIcon::pixmap(QStringLiteral("lock"), 18, 1.0);
        QVERIFY(a.cacheKey() != c.cacheKey());
    }

    void resource_svgs_match_the_design_folder()
    {
        for (const QString& name : kNames) {
            const QByteArray spec = readAll(sourcePath(
                QStringLiteral("docs/architecture/2026-10-05-radio-speaker-and-audio-setup-design/icons/%1.svg")
                    .arg(name)));
            const QByteArray resource = readAll(QStringLiteral(":/icons/emoji/%1.svg").arg(name));
            QVERIFY2(!spec.isEmpty(), qPrintable(name));
            QVERIFY2(spec == resource, qPrintable(name));
        }
    }

    void rx_applet_lock_follows_the_slice()
    {
        SliceModel slice(0);
        RxApplet applet(nullptr, nullptr);
        applet.setSlice(&slice);  // wires the slice's signals, as MainWindow does
        auto* lock = applet.findChild<QPushButton*>(QStringLiteral("RxLockButton"));
        QVERIFY(lock);
        QVERIFY(lock->text().isEmpty());
        QCOMPARE(iconName(lock), QStringLiteral("unlock"));
        capture(lock, QStringLiteral("rx-lock-unlocked"));

        slice.setLocked(true);
        QCOMPARE(iconName(lock), QStringLiteral("lock"));
        QVERIFY(lock->isChecked());
        QVERIFY(lock->text().isEmpty());
        capture(lock, QStringLiteral("rx-lock-locked"));

        slice.setLocked(false);
        QCOMPARE(iconName(lock), QStringLiteral("unlock"));

        // From the button itself: the click writes the slice and the icon.
        lock->click();
        QVERIFY(slice.locked());
        QCOMPARE(iconName(lock), QStringLiteral("lock"));
    }

    void rx_applet_lock_reads_a_locked_slice_on_build()
    {
        SliceModel slice(0);
        slice.setLocked(true);
        RxApplet applet(&slice, nullptr);
        auto* lock = applet.findChild<QPushButton*>(QStringLiteral("RxLockButton"));
        QVERIFY(lock);
        QCOMPARE(iconName(lock), QStringLiteral("lock"));
    }

    void vfo_flag_lock_follows_the_slice()
    {
        QWidget pan;
        pan.resize(800, 400);
        auto* flag = new VfoWidget(&pan);
        flag->setSliceIndex(0);
        flag->setLocked(true);  // locked before its floating buttons exist
        flag->updatePosition(400, 20);
        QPushButton* lock = flag->lockButtonForTest();
        QVERIFY(lock);
        QVERIFY(lock->text().isEmpty());
        QCOMPARE(iconName(lock), QStringLiteral("lock"));
        QVERIFY(lock->isChecked());
        capture(lock, QStringLiteral("flag-lock-locked"));

        flag->setLocked(false);
        QCOMPARE(iconName(lock), QStringLiteral("unlock"));
        capture(lock, QStringLiteral("flag-lock-unlocked"));

        QSignalSpy spy(flag, &VfoWidget::lockChanged);
        lock->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(iconName(lock), QStringLiteral("lock"));
    }

    void container_pin_shows_pin_and_pinned()
    {
        ContainerWidget container;
        container.resize(240, 160);
        auto* pin = container.findChild<QPushButton*>(QStringLiteral("ContainerPinButton"));
        QVERIFY(pin);
        QVERIFY(pin->text().isEmpty());
        QCOMPARE(iconName(pin), QStringLiteral("pin"));
        QCOMPARE(pin->toolTip(), QStringLiteral("Pin on top"));
        pin->setVisible(true);  // shown only while floating; visible for the capture
        capture(pin->parentWidget(), QStringLiteral("container-titlebar-unpinned"));

        container.setPinOnTop(true);
        QCOMPARE(iconName(pin), QStringLiteral("pinned"));
        QCOMPARE(pin->toolTip(), QStringLiteral("Pin on top"));
        capture(pin->parentWidget(), QStringLiteral("container-titlebar-pinned"));

        container.setPinOnTop(false);
        QCOMPARE(iconName(pin), QStringLiteral("pin"));
    }

    void feature_button_shows_the_bulb()
    {
        AudioEngine engine;
        TitleBar bar(&engine);
        auto* btn = bar.findChild<QPushButton*>(QStringLiteral("featureButton"));
        QVERIFY(btn);
        QCOMPARE(iconName(btn), QStringLiteral("bulb"));
        QVERIFY(!btn->icon().isNull());
        capture(btn, QStringLiteral("feature-bulb"));
    }

    void replaced_emoji_are_gone_from_the_sites()
    {
        // The padlocks and pins the icons replaced, as UTF-8, as \U escapes
        // and as \x byte escapes.
        const QStringList needles{
            QStringLiteral("\U0001F512"), QStringLiteral("\U0001F513"),
            QStringLiteral("\U0001F4CC"), QStringLiteral("\U0001F4CD"),
            QStringLiteral("\\U0001F512"), QStringLiteral("\\U0001F513"),
            QStringLiteral("\\U0001F4CC"), QStringLiteral("\\U0001F4CD"),
            QStringLiteral("\\xF0\\x9F\\x94\\x92"), QStringLiteral("\\xF0\\x9F\\x94\\x93"),
            QStringLiteral("\\xF0\\x9F\\x93\\x8C"), QStringLiteral("\\xF0\\x9F\\x93\\x8D"),
        };
        const QStringList files{
            QStringLiteral("src/gui/applets/RxApplet.cpp"),
            QStringLiteral("src/gui/widgets/VfoWidget.cpp"),
            QStringLiteral("src/gui/containers/ContainerWidget.cpp"),
            QStringLiteral("src/gui/TitleBar.cpp"),
        };
        for (const QString& file : files) {
            const QString text = QString::fromUtf8(readAll(sourcePath(file)));
            QVERIFY2(!text.isEmpty(), qPrintable(file));
            for (const QString& needle : needles) {
                QVERIFY2(!text.contains(needle, Qt::CaseInsensitive),
                         qPrintable(file + QStringLiteral(" still has ") + needle));
            }
        }
    }
};

QTEST_MAIN(TstAppIcon)
#include "tst_app_icon.moc"
