// R1 Task 4 guard test. src/core and src/models must never #include
// src/gui, and must never reach into the Qt widget / GUI rendering stack
// either: that is the invariant the headless nereusd daemon depends on to
// link NereusCore without a widget toolkit. This is a guard test, not a
// unit test -- it asserts a property of the whole tree, not one class's
// behavior, and it is meant to fail loudly the moment anyone reintroduces
// a GUI dependency into either directory.
//
// Fix round 1 findings (reviewer, see task-4-report.md):
//   Finding 1: a wrong NEREUS_SOURCE_DIR scans 0 files, finds 0 offenders,
//     and PASSES -- a silently-green boundary guard for the load-bearing
//     invariant of the whole R1 epic. `scanned` plus the QVERIFY2 floor
//     below turns "found nothing" into a hard failure unless the scan
//     actually walked a real tree (320 files as of this fix: 287 core +
//     33 models).
//   Finding 3: QString::contains(literal) only caught one exact spelling.
//     `${CMAKE_SOURCE_DIR}/src` is a PUBLIC include dir, so `#include
//     <gui/...>`, `#include "../gui/..."`, extra whitespace after `#` or
//     `include`, all compile and all slipped past the literal match. The
//     regex below is the brief's own guard test verbatim except for this
//     one line, which is why it is being fixed here rather than treated
//     as a pre-existing bug outside this task's file.
//
// R1 merge-blocker fix round: the "gui/" path rule was necessary but not
// sufficient. CMakeLists.txt links Qt6::Widgets PUBLIC onto NereusCore
// (deliberately, because the shared precompiled header covers both
// NereusCore and NereusGui and its "GUI commons" section includes
// <QWidget> / <QPainter> / <QPen> / <QBrush>), so a bare
// `#include <QWidget>` in src/core would compile, link, and be completely
// invisible to a rule that only looks for "gui/". Closing that hole in
// the guard costs nothing; dropping the Widgets link would break PCH
// compilation, and splitting the PCH is a build-performance change that
// needs its own measurement. So the deny list below carries the
// invariant instead, and the comment at the Qt6::Widgets link records
// that this test is what enforces it.
//
// no-port-check: NereusSDR-original test infrastructure.
#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace {

// Qt module directories that are GUI-stack in their entirety. Any
// `#include <QtWidgets/...>` form, including private headers and any
// class added in a future Qt release, is rejected on the prefix alone
// without needing to appear in the name list below.
//
// QtGui is deliberately absent: it is a mixed module. src/core uses
// <QColor> in 7 files (including core/spectrum/ISpectrumSink.h, where it
// is load-bearing) and <QDesktopServices> in core/SupportBundle.cpp, both
// of which are QtGui and both of which must keep passing. The GUI-stack
// half of QtGui is covered by name in kDeniedHeaders instead.
const QStringList kDeniedModuleDirs = {
    QStringLiteral("QtWidgets"),
    QStringLiteral("QtQuick"),
    QStringLiteral("QtQuickWidgets"),
    QStringLiteral("QtQml"),
    QStringLiteral("QtOpenGL"),
    QStringLiteral("QtOpenGLWidgets"),
    QStringLiteral("QtPrintSupport"),
    QStringLiteral("QtSvgWidgets"),
};

// Header basenames that mean "this file has joined the GUI stack",
// compared case-insensitively with any trailing ".h" removed, so
// <QWidget>, <qwidget.h> and <QtWidgets/qwidget.h> are all one entry.
//
// The QtWidgets block is not hand-written: it is every public class
// header shipped by the module, enumerated from the Qt install
// (Qt 6.11.0, 196 entries). Regenerate after a Qt major/minor bump with:
//
//   ls "$(qmake6 -query QT_INSTALL_HEADERS)"/QtWidgets \
//     | grep -E '^Q[A-Z][A-Za-z0-9]*$' | tr 'A-Z' 'a-z' | sort
//
// (on a framework build of Qt the directory is
// lib/QtWidgets.framework/Headers instead). Regenerating is optional
// upkeep, not a correctness requirement: kDeniedModuleDirs already
// rejects every QtWidgets header via its <QtWidgets/...> spelling, and
// the names below exist to also catch the bare <QWidget> spelling, which
// is the one someone actually types.
const QSet<QString> kDeniedHeaders = {
    // ---- QtWidgets, complete as of Qt 6.11.0 -----------------------
    "qabstractbutton", "qabstractgraphicsshapeitem", "qabstractitemdelegate",
    "qabstractitemview", "qabstractscrollarea", "qabstractslider",
    "qabstractspinbox", "qaccessiblewidget", "qaccessiblewidgetv2",
    "qapplication", "qboxlayout", "qbuttongroup", "qcalendarwidget",
    "qcheckbox", "qcolordialog", "qcolormap", "qcolumnview", "qcombobox",
    "qcommandlinkbutton", "qcommonstyle", "qcompleter", "qdatawidgetmapper",
    "qdateedit", "qdatetimeedit", "qdial", "qdialog", "qdialogbuttonbox",
    "qdockwidget", "qdoublespinbox", "qerrormessage", "qfiledialog",
    "qfileiconprovider", "qfocusframe", "qfontcombobox", "qfontdialog",
    "qformlayout", "qframe", "qgesture", "qgestureevent",
    "qgesturerecognizer", "qgraphicsanchor", "qgraphicsanchorlayout",
    "qgraphicsblureffect", "qgraphicscolorizeeffect",
    "qgraphicsdropshadoweffect", "qgraphicseffect", "qgraphicsellipseitem",
    "qgraphicsgridlayout", "qgraphicsitem", "qgraphicsitemanimation",
    "qgraphicsitemgroup", "qgraphicslayout", "qgraphicslayoutitem",
    "qgraphicslinearlayout", "qgraphicslineitem", "qgraphicsobject",
    "qgraphicsopacityeffect", "qgraphicspathitem", "qgraphicspixmapitem",
    "qgraphicspolygonitem", "qgraphicsproxywidget", "qgraphicsrectitem",
    "qgraphicsrotation", "qgraphicsscale", "qgraphicsscene",
    "qgraphicsscenecontextmenuevent", "qgraphicsscenedragdropevent",
    "qgraphicssceneevent", "qgraphicsscenehelpevent",
    "qgraphicsscenehoverevent", "qgraphicsscenemouseevent",
    "qgraphicsscenemoveevent", "qgraphicssceneresizeevent",
    "qgraphicsscenewheelevent", "qgraphicssimpletextitem",
    "qgraphicstextitem", "qgraphicstransform", "qgraphicsview",
    "qgraphicswidget", "qgridlayout", "qgroupbox", "qhboxlayout",
    "qheaderview", "qinputdialog", "qitemdelegate", "qitemeditorcreator",
    "qitemeditorcreatorbase", "qitemeditorfactory", "qkeysequenceedit",
    "qlabel", "qlayout", "qlayoutitem", "qlcdnumber", "qlineedit",
    "qlistview", "qlistwidget", "qlistwidgetitem", "qmainwindow", "qmdiarea",
    "qmdisubwindow", "qmenu", "qmenubar", "qmessagebox", "qpangesture",
    "qpinchgesture", "qplaintextdocumentlayout", "qplaintextedit",
    "qprogressbar", "qprogressdialog", "qproxystyle", "qpushbutton",
    "qradiobutton", "qrhiwidget", "qrubberband", "qscrollarea", "qscrollbar",
    "qscroller", "qscrollerproperties", "qsizegrip", "qsizepolicy",
    "qslider", "qspaceritem", "qspinbox", "qsplashscreen", "qsplitter",
    "qsplitterhandle", "qstackedlayout", "qstackedwidget",
    "qstandarditemeditorcreator", "qstatusbar", "qstyle",
    "qstyleditemdelegate", "qstylefactory", "qstylehintreturn",
    "qstylehintreturnmask", "qstylehintreturnvariant", "qstyleoption",
    "qstyleoptionbutton", "qstyleoptioncombobox", "qstyleoptioncomplex",
    "qstyleoptiondockwidget", "qstyleoptionfocusrect", "qstyleoptionframe",
    "qstyleoptiongraphicsitem", "qstyleoptiongroupbox", "qstyleoptionheader",
    "qstyleoptionheaderv2", "qstyleoptionmenuitem", "qstyleoptionmenuitemv2",
    "qstyleoptionprogressbar", "qstyleoptionrubberband",
    "qstyleoptionsizegrip", "qstyleoptionslider", "qstyleoptionspinbox",
    "qstyleoptiontab", "qstyleoptiontabbarbase",
    "qstyleoptiontabwidgetframe", "qstyleoptiontitlebar",
    "qstyleoptiontoolbar", "qstyleoptiontoolbox", "qstyleoptiontoolbutton",
    "qstyleoptionviewitem", "qstylepainter", "qstyleplugin", "qswipegesture",
    "qsystemtrayicon", "qtabbar", "qtableview", "qtablewidget",
    "qtablewidgetitem", "qtablewidgetselectionrange", "qtabwidget",
    "qtapandholdgesture", "qtapgesture", "qtextbrowser", "qtextedit",
    "qtilerules", "qtimeedit", "qtoolbar", "qtoolbox", "qtoolbutton",
    "qtooltip", "qtreeview", "qtreewidget", "qtreewidgetitem",
    "qtreewidgetitemiterator", "qundoview", "qvboxlayout", "qwhatsthis",
    "qwidget", "qwidgetaction", "qwidgetdata", "qwidgetitem",
    "qwidgetitemv2", "qwizard", "qwizardpage",

    // ---- QtGui, GUI-stack subset -----------------------------------
    // Curated rather than complete, because QtGui is a mixed module (see
    // kDeniedModuleDirs). These are the families whose presence in
    // src/core or src/models means rendering, windowing or input
    // handling has leaked out of src/gui.
    //
    // Painting and imaging
    "qpainter", "qpainterpath", "qpainterpathstroker", "qpainterstateguard",
    "qpaintdevice", "qpaintdevicewindow", "qpaintengine", "qpaintevent",
    "qpen", "qbrush", "qpixmap", "qpixmapcache", "qbitmap", "qimage",
    "qimagereader", "qimagewriter", "qicon", "qiconengine", "qpalette",
    "qregion", "qpolygon", "qtransform", "qpicture", "qstatictext",
    "qmovie", "qgradient", "qlineargradient", "qradialgradient",
    "qconicalgradient", "qcolorspace", "qcolortransform",
    // Fonts and text layout
    "qfont", "qfontdatabase", "qfontinfo", "qfontmetrics", "qfontmetricsf",
    "qglyphrun", "qrawfont", "qtextdocument", "qtextcursor", "qtextlayout",
    "qtextformat", "qtextoption", "qsyntaxhighlighter",
    // Application, windowing and surfaces
    "qguiapplication", "qwindow", "qrasterwindow", "qscreen", "qcursor",
    "qclipboard", "qdrag", "qbackingstore", "qsurface", "qsurfaceformat",
    "qoffscreensurface", "qstylehints", "qsessionmanager",
    // Input events
    "qinputevent", "qpointerevent", "qsinglepointevent", "qmouseevent",
    "qkeyevent", "qwheelevent", "qtabletevent", "qtouchevent",
    "qhoverevent", "qenterevent", "qfocusevent", "qresizeevent",
    "qmoveevent", "qshowevent", "qhideevent", "qcloseevent",
    "qexposeevent", "qcontextmenuevent", "qdragenterevent",
    "qdragmoveevent", "qdragleaveevent", "qdropevent", "qhelpevent",
    "qinputmethod", "qinputmethodevent", "qinputdevice",
    "qpointingdevice", "qnativegestureevent", "qshortcutevent",
    // Actions and shortcuts
    "qaction", "qactionevent", "qactiongroup", "qshortcut", "qkeysequence",
    // Item models that pull in the GUI stack
    "qstandarditem", "qstandarditemmodel", "qfilesystemmodel",
    // Accelerated rendering: OpenGL, Vulkan, and the RHI abstraction
    // SpectrumWidget / MeterWidget render through.
    "qopenglcontext", "qopenglcontextgroup", "qopenglfunctions",
    "qopenglextrafunctions", "qvulkaninstance", "qvulkanwindow",
    "qvulkanfunctions", "qvulkandevicefunctions",
    "qrhi", "qrhiwidget",
};

// Structure of an #include line, in any of the forms the include path
// search treats identically: <...> or "...", any amount of whitespace
// around "#" and after "include". Capture 1 is the header as written.
const QRegularExpression& includeLineRe()
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*#\\s*include\\s*[<\"]([^>\"]+)[>\"]"));
    return re;
}

// src/gui reached through the PUBLIC ${CMAKE_SOURCE_DIR}/src include dir,
// with any number of leading "../" segments and an optional "src/".
const QRegularExpression& guiPathRe()
{
    static const QRegularExpression re(
        QStringLiteral("^(\\.\\./)*(src/)?gui/"));
    return re;
}

/// Returns why `header` is forbidden, or an empty string if it is fine.
QString rejectionReason(const QString& header)
{
    if (guiPathRe().match(header).hasMatch()) {
        return QStringLiteral("includes src/gui");
    }

    const QStringList parts = header.split(QLatin1Char('/'));
    for (const QString& dir : kDeniedModuleDirs) {
        if (parts.first() == dir) {
            return QStringLiteral("includes the %1 module").arg(dir);
        }
    }

    // <QtGui/rhi/qrhi.h> and the "rhi/qrhi.h" spelling QRhiWidget users
    // reach for. Caught by path, since the basename rule below sees only
    // "qrhi.h" when the header is spelled with its directory.
    if (header.contains(QStringLiteral("rhi/qrhi"))) {
        return QStringLiteral("includes the QRhi rendering abstraction");
    }

    QString base = parts.last().toLower();
    if (base.endsWith(QStringLiteral(".h"))) {
        base.chop(2);
    }
    if (kDeniedHeaders.contains(base)) {
        return QStringLiteral("includes the GUI-stack header %1")
            .arg(parts.last());
    }

    return {};
}

}  // namespace

class TstCoreHasNoGuiIncludes : public QObject {
    Q_OBJECT
private slots:
    // src/core and src/models must never include src/gui, nor any Qt
    // widget / GUI rendering header.  This is the invariant that lets
    // nereusd link NereusCore without a widget toolkit.
    void coreNeverIncludesGui()
    {
        // The deny list going empty (a bad edit, a botched merge) would
        // make this test pass on a tree full of offenders, exactly the
        // vacuous-pass failure mode Finding 1 fixed for the file scan.
        QVERIFY2(kDeniedHeaders.size() > 150,
                 qPrintable(QString("deny list has only %1 entries, it has "
                                    "been gutted")
                                .arg(kDeniedHeaders.size())));

        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QStringList offenders;
        int scanned = 0;
        for (const QString& sub : {QStringLiteral("/src/core"), QStringLiteral("/src/models")}) {
            QDirIterator it(root + sub, {"*.h", "*.cpp", "*.mm"},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
                ++scanned;
                QTextStream ts(&f);
                int line = 0;
                while (!ts.atEnd()) {
                    ++line;
                    const auto m = includeLineRe().match(ts.readLine());
                    if (!m.hasMatch()) { continue; }
                    const QString why = rejectionReason(m.captured(1));
                    if (!why.isEmpty()) {
                        offenders << QStringLiteral("%1:%2 %3")
                                         .arg(path).arg(line).arg(why);
                    }
                }
            }
        }
        if (!offenders.isEmpty()) {
            qWarning() << "core/models reach into the GUI stack:" << offenders;
        }
        // A wrong NEREUS_SOURCE_DIR (or a src/core|src/models that stops
        // existing) scans zero files and would otherwise report a false
        // "zero offenders" pass. Fail loudly instead of passing vacuously.
        QVERIFY2(scanned > 100,
                 qPrintable(QString("only %1 files scanned, NEREUS_SOURCE_DIR is probably wrong")
                                .arg(scanned)));
        QCOMPARE(offenders.size(), 0);
    }

    // The rules themselves, so a future edit that guts one of them fails
    // here rather than quietly widening what src/core may include.
    void rejectionRulesCoverEverySpelling_data()
    {
        QTest::addColumn<QString>("header");
        QTest::addColumn<bool>("rejected");

        QTest::newRow("gui bare")        << "gui/SpectrumWidget.h"     << true;
        QTest::newRow("gui relative")    << "../gui/SpectrumWidget.h"  << true;
        QTest::newRow("gui src")         << "src/gui/SpectrumWidget.h" << true;
        QTest::newRow("widget bare")     << "QWidget"                  << true;
        QTest::newRow("widget lower")    << "qwidget.h"                << true;
        QTest::newRow("widget module")   << "QtWidgets/QWidget"        << true;
        QTest::newRow("widget private")  << "QtWidgets/private/qwidget_p.h" << true;
        QTest::newRow("painter")         << "QPainter"                 << true;
        QTest::newRow("qaction")         << "QAction"                  << true;
        QTest::newRow("rhi path")        << "rhi/qrhi.h"               << true;
        QTest::newRow("rhi gui path")    << "QtGui/rhi/qrhi.h"         << true;
        QTest::newRow("rhiwidget")       << "QRhiWidget"               << true;
        QTest::newRow("quick")           << "QtQuick/QQuickItem"       << true;

        // QtGui headers src/core legitimately uses today. These must
        // keep passing: <QColor> appears in 7 files including
        // core/spectrum/ISpectrumSink.h, and <QDesktopServices> in
        // core/SupportBundle.cpp.
        QTest::newRow("qcolor")          << "QColor"                   << false;
        QTest::newRow("qdesktopservices")<< "QDesktopServices"         << false;
        // Qt Core / Network / Multimedia, and the project's own headers.
        QTest::newRow("qstring")         << "QString"                  << false;
        QTest::newRow("qabstracttablemodel") << "QAbstractTableModel"  << false;
        QTest::newRow("qsortfilterproxy") << "QSortFilterProxyModel"   << false;
        QTest::newRow("qaudiosink")      << "QAudioSink"               << false;
        QTest::newRow("core header")     << "core/AppSettings.h"       << false;
        QTest::newRow("models header")   << "models/SliceModel.h"      << false;
    }

    void rejectionRulesCoverEverySpelling()
    {
        QFETCH(QString, header);
        QFETCH(bool, rejected);
        const QString why = rejectionReason(header);
        QVERIFY2(why.isEmpty() != rejected,
                 qPrintable(QStringLiteral("%1: expected %2, got \"%3\"")
                                .arg(header)
                                .arg(rejected ? "rejected" : "accepted")
                                .arg(why)));
    }
};

QTEST_MAIN(TstCoreHasNoGuiIncludes)
#include "tst_core_has_no_gui_includes.moc"
