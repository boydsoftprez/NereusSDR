// no-port-check: NereusSDR-original test for the workspace preview's
// colours on Settings pages and dialogs (src/gui/styles: QssTranslator,
// StylesheetColourTable, WorkspacePreview::roleColour and dataBackground).
//
// With the workspace look on, runs every dialog scene of the visual
// preview tool (tests/visual/PreviewScenes.cpp: the dialog-* scenes and
// every Setup page, tab, scrolled and size scene, which the tool captures
// as look-dialog-*), grabs each dialog as the scene does, and reports every
// colour painted that is outside the workspace palette, with the widget
// that paints it.
//
// A colour is inside the palette when it is a palette role, or lies on
// the straight line between a palette role and a surface role
// (antialiased text and edges, gradients between roles), or between a role and the glass light
// (white, up to kWhiteBlendMax) or the glass shade (black, up to
// kBlackBlendMax). A colour is reported when at least kMinPixels pixels of
// one grab carry it. Inside a flat area the rule is stricter
// (PaletteCheck::insideFill): a role or a step of one of the look's
// gradients. User-chosen and identity colours are exempt and
// listed in kExemptClasses and kExemptColours; data surfaces (D22) are
// checked for their background only, their ink is exempt
// (kDataSurfaceClasses).
//
// NEREUS_COLOUR_SWEEP_SCENES (comma-separated globs) limits the scenes;
// NEREUS_COLOUR_SWEEP_REPORT names a TSV file the findings are written to;
// NEREUS_COLOUR_SWEEP_IMAGES names a folder the grabs are saved in.
// NEREUS_COLOUR_SWEEP_GROUP=scenes1 to scenes6 runs one sixth of the scenes
// (main() below); ctest runs the six as entries side by side.
//
// Modification history (NereusSDR):
//   2026-10-09 - Written for the colour round on Settings pages and
//                 dialogs. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-10 - JJ's colour answers: the lists split into kept by
//                 decision (Block TX tint, TX filter label, the PureSignal
//                 badge's DimGray and DodgerBlue, D32) and stopped for JJ;
//                 the lights, the HL2 pins, the LAN scan bar and the NR2
//                 rim left the stopped list. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-10 - The connection pill left the stopped list: its states
//                 take their meaning, stale the warn colour. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10 - JJ's option 1: the default button's rim and the LAN scan
//                 list's current cell left the stopped list; the default
//                 button's tint is kept by decision. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-10 - Merging the two: nothing is stopped for JJ, so the
//                 stopped list is empty (a zero-length std::array). J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Linux: text is drawn with grey antialiasing whatever the
//                 system's fontconfig says (a config that asks for
//                 subpixel rendering gave every glyph red and blue edges,
//                 which are no colour of the app). A scrolled scene whose
//                 page fits with this platform's fonts is logged, not
//                 failed. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-14 - Treat the alpha colour dialog's current-colour preview as
//                 the user-owned fill colour it displays. J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-16 - The scenes run as six ctest entries, a sixth each, so
//                 CI runs them side by side (CI speed round). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iterator>
#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/TgxlConnection.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/PsaIndicatorWidget.h"
#include "gui/applets/AmpApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/applets/TunerApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/TunerModel.h"
#include "visual/PreviewScenes.h"
#include "TestFunctionGroups.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

// A colour this far (per channel, after rounding) from a palette role or
// from the line between two is still the palette.
constexpr double kTolerance = 1.5;
// The glass light is white at alpha 18 (kGlassLightAlpha); allow a little
// more for stacked edges.
constexpr double kWhiteBlendMax = 0.12;
// The glass shade is black at alpha 160 (kGlassShade).
constexpr double kBlackBlendMax = 0.70;
// A fill (a pixel inside a flat area) is this close to a role.
constexpr double kFillTolerance = 1.0;
// raisedFillOf()'s lightness lift and drop (WorkspaceComponents.cpp
// kRaisedLift, and three quarters of it).
constexpr int kRaisedLift = 16;
constexpr int kRaisedDrop = kRaisedLift * 3 / 4;
// Antialiasing and gradients blend a colour with the surface under it:
// a blend counts only when one of its two roles is a surface (HSL
// lightness below this). Two bright roles (a red and a green) never
// blend, and their mixtures would pass for neutral greys.
constexpr double kSurfaceLightness = 0.40;
// Fewer pixels of one colour in one grab are corner antialiasing.
constexpr int kMinPixels = 6;
// Enough pixel positions to name the widget that paints a colour.
constexpr int kOwnerSamples = 400;

// Class names without their namespace (unqualified()).
// User-chosen colours, identity colours and meter faces: never checked.
const QStringList kExemptClasses = {
    // Colors & Theme and every colour pick: the user's colours.
    QStringLiteral("ColorSwatchButton"),
    // The colour dialog's own pickers (dialog-overlay-colour).
    QStringLiteral("QColorPicker"),
    QStringLiteral("QColorLuminancePicker"),
    QStringLiteral("QColorShowLabel"),
    QStringLiteral("QColorWell"),
    QStringLiteral("QWellArray"),
    // Meter faces (a container's meter artwork, the bar gauges and the
    // VOX/DEXP peak meter).
    QStringLiteral("MeterWidget"),
    QStringLiteral("HGauge"),
    QStringLiteral("DexpPeakMeter"),
};

// By object name: the platform's message box icon (artwork) and the
// user's spot colours (SpotHubDialog.cpp:876, :1123 and the WSJT-X and
// FreeDV pickers, each drawn by swatchStyle()).
const QRegularExpression kExemptObjects(QStringLiteral(
    "^(qt_msgboxex_icon_label|clusterColorBtn|rbnColorBtn|freedvColorBtn|wsjtxColor\\w+)$"));

// Identity colours: the slice colours (ControlRanges kSliceColours) on
// the A/B badges and slice buttons.
const QStringList kExemptColours = {
    QStringLiteral("#00d4ff"), QStringLiteral("#ff40ff"), QStringLiteral("#40ff40"),
    QStringLiteral("#ffff00"), QStringLiteral("#ffa000"), QStringLiteral("#00e0c0"),
    QStringLiteral("#ff6080"), QStringLiteral("#b080ff"),
};

// Artwork: one widget of one scene, by its geometry path (a wildcard).
struct Artwork {
    const char* scene;
    const char* path;
    const char* what;
};
const Artwork kExemptArtwork[] = {
    { "dialog-about", "0/0", "the application icon (AboutDialog.cpp:153-157)" },
    { "dialog-container-settings", "0/3/0/0/*/1",
      "a tile's snapshot of the existing view, scaled smoothly (ContainerContentRegistry.cpp:479); "
      "the first is the S-meter face" },
};

// Data surfaces (D22): the background follows the container's role and
// is checked; traces, curves and plotted data (ink) are exempt. The
// background is the dark part of the surface (HSL lightness below
// kDataBackgroundLightness): when more than kDataBackgroundShare of the
// surface is dark and outside the palette, its background is reported.
constexpr double kDataBackgroundLightness = 0.25;
constexpr double kDataBackgroundShare = 0.25;
const QStringList kDataSurfaceClasses = {
    QStringLiteral("DiversityRadarWidget"),
    QStringLiteral("ParametricEqWidget"),
    QStringLiteral("AmpViewChart"),
};
// Data surfaces by object name: the remote diagnostics traffic graphs
// (RemoteDiagnosticsDialog.cpp:147-150), whose traces are ink.
const QStringList kDataSurfaceObjects = {
    QStringLiteral("remoteTotalTrafficGraph"),
    QStringLiteral("remoteRadioLinkGraph"),
};

// Findings kept by decision and findings stopped for JJ. Both are matched
// by image and owner (wildcards) and colour ("*": any), counted and
// printed, not failed. A finding on neither list fails the sweep.
struct Listed {
    const char* image;
    const char* owner;
    const char* colour;
    const char* why;
};
// Kept by decision (JJ's colour answers, 2026-10-10): the colour stays
// outside the palette on purpose.
const Listed kKeptByDecision[] = {
    { "look-dialog-setup-hardware-config--antenna-alex*", "QFrame", "#140f12",
      "kept (settled 2026-10-10): the Block TX safety frame, danger at the source's 8 percent alpha "
      "(AntennaAlexAntennaControlTab.cpp:302), a translucent tint the analyser does not model" },
    { "look-dialog-setup-hardware-config--antenna-alex*", "QLabel", "#1f161a",
      "kept (settled 2026-10-10): the Block TX safety note over that tint "
      "(AntennaAlexAntennaControlTab.cpp:322)" },
    { "look-dialog-lit-psa-on", "QLabel", "#ffaa70",
      "kept (settled 2026-10-10): the TX filter overlay label is the user's colour setting "
      "(StyleConstants:388)" },
    { "look-dialog-overlay-colour", "QPushButton", "#00e5ff",
      "kept under the sweep's user-colour rule: Qt's alpha colour dialog paints its current-colour "
      "preview as a plain QPushButton; this is the DisplayFillColor default, not dialog chrome" },
    { "look-dialog-*", "QPushButton*", "#212630",
      "kept (JJ 2026-10-10, option 1): the tinted fill Fusion draws on a dialog's default button from the "
      "palette Highlight. Only the rim changed, to the selection rim (Workspace::LookStyle); the fill, the "
      "shape and the text are Fusion's" },
};
// Stopped for JJ: the meaning or the mechanism is his to choose. Empty
// since 2026-10-10: the connection pill, the default button and the LAN
// scan list's current cell were all answered. To stop a finding for JJ
// again, raise the count and add a { image, owner, colour, why } entry as
// in kKeptByDecision; the sweep counts and prints it without failing.
const std::array<Listed, 0> kStoppedForJJ = {};

const Listed* listedIn(const Listed* first, const Listed* last, const QString& image, const QString& owner,
                       const QString& colour)
{
    for (const Listed* st = first; st != last; ++st) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(QString::fromLatin1(st->image)));
        const QRegularExpression ownerRe(
            QRegularExpression::wildcardToRegularExpression(QString::fromLatin1(st->owner)));
        if (re.match(image).hasMatch() && ownerRe.match(owner).hasMatch()
            && (qstrcmp(st->colour, "*") == 0 || colour == QLatin1String(st->colour))) {
            return st;
        }
    }
    return nullptr;
}

const Listed* keptFor(const QString& image, const QString& owner, const QString& colour)
{
    return listedIn(std::begin(kKeptByDecision), std::end(kKeptByDecision), image, owner, colour);
}

const Listed* stoppedFor(const QString& image, const QString& owner, const QString& colour)
{
    return listedIn(kStoppedForJJ.data(), kStoppedForJJ.data() + kStoppedForJJ.size(), image, owner, colour);
}

QString unqualified(const QString& className)
{
    const int at = className.lastIndexOf(QLatin1String("::"));
    return at < 0 ? className : className.mid(at + 2);
}

struct Widget {
    QString path;
    int depth{0};
    QString className;
    QString objectName;
    QRect rect;   // device px in the grab
};

// One widget of one grab that paints colours outside the palette: the
// most frequent of them, how many pixels in all, and how many colours.
struct Finding {
    QString image;
    QString colour;
    int pixels{0};
    QString owner;
    int colours{0};
};

QVector<Widget> parseGeometry(const QStringList& lines, double dpr)
{
    QVector<Widget> out;
    for (const QString& line : lines) {
        const QStringList f = line.split(QLatin1Char('\t'));
        if (f.size() < 8) {
            continue;
        }
        Widget w;
        w.path = f.at(0);
        w.depth = int(f.at(0).count(QLatin1Char('/')));
        w.className = f.at(1);
        w.objectName = f.at(2);
        w.rect = QRect(int(std::lround(f.at(4).toInt() * dpr)), int(std::lround(f.at(5).toInt() * dpr)),
                       int(std::lround(f.at(6).toInt() * dpr)), int(std::lround(f.at(7).toInt() * dpr)));
        out.append(w);
    }
    return out;
}

// The deepest widget that holds the point.
const Widget* ownerAt(const QVector<Widget>& widgets, const QPoint& p)
{
    const Widget* best = nullptr;
    for (const Widget& w : widgets) {
        if (w.rect.contains(p) && (best == nullptr || w.depth >= best->depth)) {
            best = &w;
        }
    }
    return best;
}

class PaletteCheck
{
public:
    PaletteCheck()
    {
        for (const auto& e : Workspace::WorkspacePalette::glassAndDepth().entries()) {
            const QColor c(QString::fromLatin1(e.hex));
            if (!m_roles.contains(c.rgb())) {
                m_roles.append(c.rgb());
                m_surface.append(c.lightnessF() < kSurfaceLightness);
            }
        }
        // The look's gradients (WorkspaceStyle glassPanelFill, titleBarStyle,
        // WorkspaceComponents vgrad and raisedFillOf).
        const auto role = [](const char* name) {
            return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name)).rgb();
        };
        m_gradients = {
            {role("panel-top"), role("panel-bottom")},
            {role("raised-top"), role("raised-bottom")},
            {role("selected-top"), role("selected-bottom")},
            {role("titlebar-top"), role("titlebar-mid")},
            {role("titlebar-mid"), role("titlebar-bottom")},
            {role("inset-top"), role("inset")},
        };
        // The lit forms of a checked green or amber button (D15;
        // WorkspaceComponents.cpp:184-191 and litGreenFill, litGreenText,
        // litGreenRim, litTreatment): a raised fill on the green lifted 14,
        // the rim lifted 16 with its top edge 16 more, the text lifted 80;
        // amber on its own background and border.
        {
            using namespace Workspace;
            const QColor greenRim(shiftL(QColor(QString::fromLatin1(kDspToggleBorder)), 16));
            for (const QColor& c : {
                     QColor(shiftL(QColor(QString::fromLatin1(kGreenBg)), 14)),
                     greenRim,
                     QColor(shiftL(greenRim, 16)),
                     QColor(shiftL(QColor(QString::fromLatin1(kLiveGreen)), 80)),
                     QColor(shiftL(QColor(QString::fromLatin1(kAmberBorder)), 16)) }) {
                if (!m_roles.contains(c.rgb())) {
                    m_roles.append(c.rgb());
                    m_surface.append(c.lightnessF() < kSurfaceLightness);
                }
            }
        }
        const int roleCount = int(m_roles.size());
        for (int i = 0; i < roleCount; ++i) {
            const QColor base(m_roles[i]);
            const QRgb lift = QColor(Workspace::shiftL(base, kRaisedLift)).rgb();
            const QRgb drop = QColor(Workspace::shiftL(base, -kRaisedDrop)).rgb();
            m_gradients.append({lift, m_roles[i]});
            m_gradients.append({m_roles[i], drop});
            for (QRgb shifted : {lift, drop}) {
                if (!m_roles.contains(shifted)) {
                    m_roles.append(shifted);
                    m_surface.append(QColor(shifted).lightnessF() < kSurfaceLightness);
                }
            }
        }
    }

    // A pixel inside a flat area (its eight neighbours the same colour) is
    // a fill, not an edge: it must be a role, or a step of one of the
    // look's gradients. Blends with text or the glass light, which an
    // edge may be, do not count: a flat grey matches the glass light over
    // a panel to within a level, and only its area tells them apart.
    bool insideFill(QRgb rgb)
    {
        rgb |= 0xff000000u;
        const auto it = m_fillCache.constFind(rgb);
        if (it != m_fillCache.constEnd()) {
            return it.value();
        }
        const double c[3] = {double(qRed(rgb)), double(qGreen(rgb)), double(qBlue(rgb))};
        bool ok = false;
        for (QRgb r : std::as_const(m_roles)) {
            const double a[3] = {double(qRed(r)), double(qGreen(r)), double(qBlue(r))};
            ok = ok || toSegment(c, a, a, 0.0) <= kFillTolerance;
        }
        for (const auto& [top, bottom] : std::as_const(m_gradients)) {
            const double a[3] = {double(qRed(top)), double(qGreen(top)), double(qBlue(top))};
            const double b[3] = {double(qRed(bottom)), double(qGreen(bottom)), double(qBlue(bottom))};
            ok = ok || toSegment(c, a, b, 1.0) <= kTolerance;
        }
        m_fillCache.insert(rgb, ok);
        return ok;
    }

    bool inside(QRgb rgb)
    {
        rgb |= 0xff000000u;
        const auto it = m_cache.constFind(rgb);
        if (it != m_cache.constEnd()) {
            return it.value();
        }
        const bool ok = compute(rgb);
        m_cache.insert(rgb, ok);
        return ok;
    }

private:
    // Distance (max over channels) from c to the segment a + t (b - a),
    // t in [0, tMax].
    static double toSegment(const double c[3], const double a[3], const double b[3], double tMax)
    {
        double num = 0.0;
        double den = 0.0;
        for (int i = 0; i < 3; ++i) {
            num += (c[i] - a[i]) * (b[i] - a[i]);
            den += (b[i] - a[i]) * (b[i] - a[i]);
        }
        const double t = den > 0.0 ? std::clamp(num / den, 0.0, tMax) : 0.0;
        double worst = 0.0;
        for (int i = 0; i < 3; ++i) {
            worst = std::max(worst, std::abs(c[i] - (a[i] + t * (b[i] - a[i]))));
        }
        return worst;
    }

    bool compute(QRgb rgb) const
    {
        const double c[3] = {double(qRed(rgb)), double(qGreen(rgb)), double(qBlue(rgb))};
        const double white[3] = {255.0, 255.0, 255.0};
        const double black[3] = {0.0, 0.0, 0.0};
        for (int i = 0; i < m_roles.size(); ++i) {
            const double a[3] = {double(qRed(m_roles[i])), double(qGreen(m_roles[i])), double(qBlue(m_roles[i]))};
            if (toSegment(c, a, white, kWhiteBlendMax) <= kTolerance
                || toSegment(c, a, black, kBlackBlendMax) <= kTolerance) {
                return true;
            }
            for (int j = i + 1; j < m_roles.size(); ++j) {
                if (!m_surface[i] && !m_surface[j]) {
                    continue;
                }
                const double b[3] = {double(qRed(m_roles[j])), double(qGreen(m_roles[j])),
                                     double(qBlue(m_roles[j]))};
                if (toSegment(c, a, b, 1.0) <= kTolerance) {
                    return true;
                }
            }
        }
        return false;
    }

    QVector<QRgb> m_roles;
    QVector<bool> m_surface;
    QVector<QPair<QRgb, QRgb>> m_gradients;
    QHash<QRgb, bool> m_cache;
    QHash<QRgb, bool> m_fillCache;
};

// Every colour of `image` outside the palette, with the widget that paints
// most of it. `geometry` is the scene's widget list (PreviewScenes
// widgetGeometry), in the grabbed widget's logical px.
QVector<Finding> analyse(PaletteCheck& palette, const QString& name, const QImage& source,
                         const QStringList& geometry)
{
    const QImage image = source.convertToFormat(QImage::Format_RGB32);
    const double dpr = source.devicePixelRatio() > 0.0 ? source.devicePixelRatio() : 1.0;
    const QVector<Widget> widgets = parseGeometry(geometry, dpr);

    // Exempt rectangles, and the data surfaces whose background alone is
    // checked: `true` when that background is outside the palette.
    QVector<QRect> exempt;
    QVector<QPair<QRect, bool>> surfaces;
    const auto dark = [](QRgb px) { return QColor(px).lightnessF() < kDataBackgroundLightness; };
    for (const Widget& w : widgets) {
        bool artwork = false;
        for (const Artwork& a : kExemptArtwork) {
            const QString scene = QString::fromLatin1(a.scene);
            artwork = artwork
                || ((name == scene || name == QStringLiteral("look-") + scene)
                    && QRegularExpression(QRegularExpression::wildcardToRegularExpression(
                           QString::fromLatin1(a.path))).match(w.path).hasMatch());
        }
        if (artwork || kExemptClasses.contains(unqualified(w.className))
            || kExemptObjects.match(w.objectName).hasMatch()) {
            exempt.append(w.rect);
        } else if (kDataSurfaceClasses.contains(unqualified(w.className))
                   || kDataSurfaceObjects.contains(w.objectName)) {
            const QRect r = w.rect.intersected(image.rect());
            qint64 outside = 0;
            for (int y = r.top(); y <= r.bottom(); ++y) {
                const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
                for (int x = r.left(); x <= r.right(); ++x) {
                    const QRgb px = line[x] | 0xff000000u;
                    if (dark(px) && !palette.inside(px)) {
                        ++outside;
                    }
                }
            }
            const qint64 area = qint64(r.width()) * r.height();
            surfaces.append({r, area > 0 && double(outside) / double(area) > kDataBackgroundShare});
        }
    }
    QSet<QRgb> exemptColours;
    for (const QString& hex : kExemptColours) {
        exemptColours.insert(QColor(hex).rgb());
    }

    struct Hit {
        int pixels{0};
        QVector<QPoint> samples;
    };
    QHash<QRgb, Hit> hits;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb px = line[x] | 0xff000000u;
            if (exemptColours.contains(px)) {
                continue;
            }
            bool fill = x > 0 && y > 0 && x + 1 < image.width() && y + 1 < image.height();
            for (int dy = -1; fill && dy <= 1; ++dy) {
                const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y + dy));
                for (int dx = -1; fill && dx <= 1; ++dx) {
                    fill = (row[x + dx] | 0xff000000u) == px;
                }
            }
            if (fill ? palette.insideFill(px) : palette.inside(px)) {
                continue;
            }
            const QPoint p(x, y);
            bool skip = false;
            for (const QRect& r : exempt) {
                skip = skip || r.contains(p);
            }
            for (const auto& [r, flagged] : surfaces) {
                skip = skip || (r.contains(p) && !(flagged && dark(px)));
            }
            if (skip) {
                continue;
            }
            Hit& h = hits[px];
            h.pixels += 1;
            if (h.samples.size() < kOwnerSamples) {
                h.samples.append(p);
            }
        }
    }

    // Group by the widget that paints most of each colour.
    struct Group {
        int pixels{0};
        int colours{0};
        QRgb top{0};
        int topPixels{-1};
    };
    QHash<QString, Group> groups;
    for (auto it = hits.constBegin(); it != hits.constEnd(); ++it) {
        QHash<QString, int> owners;
        for (const QPoint& p : it.value().samples) {
            const Widget* w = ownerAt(widgets, p);
            owners[w == nullptr ? QStringLiteral("?")
                                : (w->objectName.isEmpty() ? w->className
                                                           : w->className + QLatin1Char('#') + w->objectName)] += 1;
        }
        QString owner;
        int most = -1;
        for (auto o = owners.constBegin(); o != owners.constEnd(); ++o) {
            if (o.value() > most || (o.value() == most && o.key() < owner)) {
                most = o.value();
                owner = o.key();
            }
        }
        Group& g = groups[owner];
        g.pixels += it.value().pixels;
        g.colours += 1;
        if (it.value().pixels > g.topPixels) {
            g.topPixels = it.value().pixels;
            g.top = it.key();
        }
    }
    QVector<Finding> out;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if (it.value().topPixels < kMinPixels) {
            continue;
        }
        out.append({name, QColor(it.value().top).name(), it.value().pixels, it.key(), it.value().colours});
    }
    std::sort(out.begin(), out.end(), [](const Finding& a, const Finding& b) {
        return a.pixels != b.pixels ? a.pixels > b.pixels : a.owner < b.owner;
    });
    return out;
}

// The scenes run as kSceneEntries ctest entries, scenes1 to scenes6
// (tests/CMakeLists.txt), each the same binary with
// NEREUS_COLOUR_SWEEP_GROUP naming it. 2026-10-16 CI speed round: the
// whole walk took 549 s on the Linux runner, one entry alone on its shard.
constexpr int kSceneEntries = 6;
constexpr const char* kSceneGroupVariable = "NEREUS_COLOUR_SWEEP_GROUP";

// The part of `scenes` this run checks: all of them, or for entry scenesK
// the K-th of kSceneEntries consecutive runs of the list, so each scene
// still follows the one it follows in a whole walk except at a cut, and
// together the entries cover every scene once.
QVector<Preview::Scene> scenePart(const QVector<Preview::Scene>& scenes)
{
    const QString group = qEnvironmentVariable(kSceneGroupVariable);
    for (int k = 0; k < kSceneEntries; ++k) {
        if (group == QStringLiteral("scenes%1").arg(k + 1)) {
            const qsizetype begin = scenes.size() * k / kSceneEntries;
            const qsizetype end = scenes.size() * (k + 1) / kSceneEntries;
            return scenes.mid(begin, end - begin);
        }
    }
    return scenes;
}

bool sceneSelected(const QString& name)
{
    const QString filter = qEnvironmentVariable("NEREUS_COLOUR_SWEEP_SCENES");
    if (filter.isEmpty()) {
        return true;
    }
    for (const QString& glob : filter.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(glob.trimmed()));
        if (re.match(name).hasMatch() || re.match(QStringLiteral("look-") + name).hasMatch()) {
            return true;
        }
    }
    return false;
}

void writeReport(const QVector<Finding>& findings, const char* variable = "NEREUS_COLOUR_SWEEP_REPORT")
{
    const QString path = qEnvironmentVariable(variable);
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning("cannot write %s", qPrintable(path));
        return;
    }
    QTextStream out(&f);
    out << "image\towner\tcolour\tpixels\tcolours\n";
    for (const Finding& x : findings) {
        out << x.image << '\t' << x.owner << '\t' << x.colour << '\t' << x.pixels << '\t' << x.colours << '\n';
    }
}

QString describe(const QVector<Finding>& findings)
{
    QStringList lines;
    for (const Finding& x : findings) {
        lines << QStringLiteral("%1: %2 (%3 px in %4 colours, most %5)")
                     .arg(x.image, x.owner).arg(x.pixels).arg(x.colours).arg(x.colour);
    }
    return lines.join(QLatin1Char('\n'));
}

// Why a toggled applet button's colours are not the palette's to judge,
// or empty: a slice tab is that slice's identity color
// (RxApplet.cpp:1838-1844), which the controller exempted. The slice flag
// is judged like any applet since it takes the look (JJ, 2026-10-11).
QString exemptReason(QWidget* w)
{
    const QString sheet = w->styleSheet();
    for (int i = 0; i < 8; ++i) {
        if (sheet.contains(QStringLiteral("color: %1;").arg(NereusSDR::VfoWidget::sliceColor(i).name()))) {
            return QStringLiteral("slice tab (slice identity colour)");
        }
    }
    return QString();
}

// The applet that holds a widget: its nearest ancestor whose class name
// ends in "Applet", or null.
QWidget* appletOf(QWidget* w)
{
    for (QWidget* p = w->parentWidget(); p != nullptr; p = p->parentWidget()) {
        if (QLatin1String(p->metaObject()->className()).endsWith(QLatin1String("Applet"))) {
            return p;
        }
    }
    return nullptr;
}

// One widget as the applet that holds it paints it, analysed alone. The
// grab is of the holder, so a rounded corner shows the applet behind it.
QVector<Finding> analyseInPlace(PaletteCheck& palette, const QString& name, QWidget* holder, QWidget* w)
{
    QApplication::processEvents();
    const QRect r(w->mapTo(holder, QPoint(0, 0)), w->size());
    const QImage img = holder->grab(r).toImage();
    QString label = w->objectName();
    if (label.isEmpty()) {
        if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            label = b->text().simplified();
        }
    }
    label.replace(QLatin1Char('\t'), QLatin1Char(' '));
    const QStringList geometry = {
        QStringLiteral("0\t%1\t%2\tother\t0\t0\t%3\t%4")
            .arg(QLatin1String(w->metaObject()->className()), label)
            .arg(r.width()).arg(r.height()),
    };
    return analyse(palette, name, img, geometry);
}

} // namespace

class TstWorkspaceColourSweep : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus*.debug=false\nnereus*.info=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("workspace-colour-sweep")));
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{30});
        auto& app = *qobject_cast<QApplication*>(QCoreApplication::instance());
        // The startup sequence of src/main.cpp with the look on.
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        WorkspacePreview::setEnabledForTest(true);
        applyDarkPalette(app);
        applyAppBaselineQss(app);
        applyWorkspacePalette(app);
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
        RadioDiscovery::clearHoldOffForTest();
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // The analysis sees a colour planted outside the palette, names the
    // widget, and passes a palette-only widget.
    void analyserFlagsAPlantedColour()
    {
        PaletteCheck palette;
        QWidget host;
        host.setObjectName(QStringLiteral("host"));
        host.setStyleSheet(QStringLiteral("QWidget#host { background: #08080a; }"));
        auto* layout = new QVBoxLayout(&host);
        auto* clean = new QLabel(QStringLiteral("Palette text on the page"), &host);
        clean->setStyleSheet(QStringLiteral("QLabel { color: #dcdce1; background: #08080a; }"));
        layout->addWidget(clean);
        host.resize(320, 120);
        host.show();
        QCoreApplication::processEvents();
        const QStringList geometry = {
            QStringLiteral("0\tQWidget\thost\tother\t0\t0\t320\t120"),
        };
        const QVector<Finding> before = analyse(palette, QStringLiteral("clean"), host.grab().toImage(), geometry);
        QVERIFY2(before.isEmpty(), qPrintable(describe(before)));

        auto* planted = new QLabel(QStringLiteral("Planted"), &host);
        planted->setObjectName(QStringLiteral("planted"));
        planted->setAutoFillBackground(true);
        planted->setStyleSheet(QStringLiteral("QLabel { color: #dcdce1; background: #12ab34; }"));
        layout->addWidget(planted);
        QCoreApplication::processEvents();
        layout->activate();
        const QRect r = planted->geometry();
        const QStringList withPlanted = geometry + QStringList{
            QStringLiteral("0/1\tQLabel\tplanted\tother\t%1\t%2\t%3\t%4")
                .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())};
        const QVector<Finding> after = analyse(palette, QStringLiteral("planted"), host.grab().toImage(), withPlanted);
        QVERIFY(!after.isEmpty());
        QCOMPARE(after.first().colour, QStringLiteral("#12ab34"));
        QCOMPARE(after.first().owner, QStringLiteral("QLabel#planted"));
    }

    // Antialiased text between two roles and a gradient between two roles
    // are the palette; an off-palette fill grey and two of today's colours
    // are not.
    void blendsBetweenRolesAreThePalette()
    {
        PaletteCheck palette;
        const QColor text(QStringLiteral("#dcdce1"));
        const QColor page(QStringLiteral("#08080a"));
        for (double t : {0.1, 0.33, 0.5, 0.8}) {
            const QColor mix(int(std::lround(page.red() + t * (text.red() - page.red()))),
                             int(std::lround(page.green() + t * (text.green() - page.green()))),
                             int(std::lround(page.blue() + t * (text.blue() - page.blue()))));
            QVERIFY2(palette.inside(mix.rgb()), qPrintable(mix.name()));
        }
        // A flat grey between the panel and the glass light is not a fill
        // of the palette; a step of the raised gradient is.
        QVERIFY(!palette.insideFill(QColor(QStringLiteral("#1e1e1e")).rgb()));
        QVERIFY(palette.insideFill(QColor(QStringLiteral("#1b1b1f")).rgb()));
        QVERIFY(palette.insideFill(QColor(QStringLiteral("#08080a")).rgb()));
        QVERIFY(!palette.inside(QColor(QStringLiteral("#00b4d8")).rgb()));
        QVERIFY(!palette.inside(QColor(QStringLiteral("#ffcc66")).rgb()));
    }

    // Every dialog the preview tool's scenes cover, with the look on.
    void settingsAndDialogsUseThePalette()
    {
        AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), QStringLiteral("2v"));
        auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                                   MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window->radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        window->resize(1512, 869);
        window->show();
        window->resize(1512, 869);
        model->setConnectionStateForTest(ConnectionState::Connected);
        Preview::pump(1500);

        QVector<Preview::Scene> scenes;
        for (const Preview::Scene& s : Preview::fixedScenes()) {
            if (s.dialog && s.name.startsWith(QLatin1String("dialog-")) && sceneSelected(s.name)) {
                scenes.append(s);
            }
        }
        for (const Preview::Scene& s : Preview::setupPageScenes(window.get())) {
            if (sceneSelected(s.name)) {
                scenes.append(s);
            }
        }
        scenes = scenePart(scenes);
        QVERIFY(!scenes.isEmpty());

        PaletteCheck palette;
        QVector<Finding> findings;
        int images = 0;
        QStringList failed;
        for (const Preview::Scene& s : scenes) {
            // The preview tool runs each scene in a fresh process on the
            // Saturn board; here the window is shared, so a scene posed on
            // the Hermes Lite 2 must not leave the next one on it
            // (openSetupTabScene sets the board only when it is not Saturn).
            model->setBoardForTest(HPSDRHW::Saturn);
            Preview::SceneContext ctx;
            ctx.window = window.get();
            ctx.model = model;
            ctx.size = QSize(1512, 869);
            ctx.dpr = 1.0;
            s.run(ctx);
            if (ctx.nothingToScroll) {
                // The page fits with this platform's fonts, so its own
                // scene (not --scrolled) has grabbed the whole of it.
                qInfo().noquote() << "nothing to scroll, covered by the page's own scene:" << s.name
                                  << ctx.failure;
                continue;
            }
            if (!ctx.failure.isEmpty()) {
                failed << QStringLiteral("%1: %2").arg(s.name, ctx.failure);
                continue;
            }
            for (const Preview::CapturedImage& img : ctx.images) {
                if (img.kind != QLatin1String("dialog")) {
                    continue;
                }
                ++images;
                const QString dump = qEnvironmentVariable("NEREUS_COLOUR_SWEEP_IMAGES");
                if (!dump.isEmpty()) {
                    img.image.save(dump + QStringLiteral("/look-") + img.name + QStringLiteral(".png"));
                }
                findings += analyse(palette, QStringLiteral("look-") + img.name, img.image, img.geometry);
            }
        }
        writeReport(findings);
        QSet<QString> colours;
        QSet<QString> owners;
        QVector<Finding> unexpected;
        int stopped = 0;
        int kept = 0;
        for (const Finding& f : findings) {
            colours.insert(f.colour);
            owners.insert(f.owner);
            if (keptFor(f.image, f.owner, f.colour) != nullptr) {
                ++kept;
            } else if (stoppedFor(f.image, f.owner, f.colour) != nullptr) {
                ++stopped;
            } else {
                unexpected.append(f);
            }
        }
        qInfo("colour sweep: %d scenes, %d images, %d findings, %d colours, %d widgets, %d scenes failed",
              int(scenes.size()), images, int(findings.size()), int(colours.size()), int(owners.size()),
              int(failed.size()));
        qInfo("colour sweep: %d findings kept by decision, %d stopped for JJ, %d on neither list", kept, stopped,
              int(unexpected.size()));
        QVERIFY2(failed.isEmpty(), qPrintable(failed.join(QLatin1Char('\n'))));
        QVERIFY(images > 0);
        QVERIFY2(unexpected.isEmpty(), qPrintable(describe(unexpected)));
    }
    // The applets' states after the live pass. Every visible checkable
    // button in the main window's applets is toggled with its signals
    // blocked (so no toggle reaches MOX or a connection) and painted in
    // place; then the states the applets style from their models are posed
    // through their setters on standalone applets: OPERATE on in the tuner,
    // PGXL and RF2K-S applets, the selected tuner antenna, PS-A checked and
    // each PureSignal badge fill.
    void appletStatesUseThePalette()
    {
        PaletteCheck palette;
        QVector<Finding> findings;
        int poses = 0;
        QMap<QString, int> exempted;
        {
            AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), QStringLiteral("2v"));
            auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                                       MainWindow::ConnectionStartup::Deferred);
            RadioModel* model = window->radioModel();
            model->setBoardForTest(HPSDRHW::Saturn);
            model->configureStreamPool(5, 5, 192000);
            window->resize(1512, 869);
            window->show();
            model->setConnectionStateForTest(ConnectionState::Connected);
            Preview::pump(1500);
            for (QAbstractButton* b : window->findChildren<QAbstractButton*>()) {
                QWidget* applet = appletOf(b);
                if (applet == nullptr || !b->isCheckable() || !b->isVisibleTo(window.get())
                    || !applet->isVisible()) {
                    continue;
                }
                const QString reason = exemptReason(b);
                if (!reason.isEmpty()) {
                    exempted[reason] += 1;
                    continue;
                }
                const bool was = b->isChecked();
                for (bool state : { !was, was }) {
                    {
                        const QSignalBlocker block(b);
                        b->setChecked(state);
                    }
                    ++poses;
                    findings += analyseInPlace(palette,
                        QStringLiteral("applet-%1-%2").arg(QLatin1String(applet->metaObject()->className()),
                                                           state ? QStringLiteral("checked")
                                                                 : QStringLiteral("unchecked")),
                        applet, b);
                }
            }
        }
        const auto settle = [](QWidget& w) {
            w.show();
            QApplication::processEvents();
            QTest::qWait(30);
            QApplication::processEvents();
        };
        {
            TgxlConnection connection;
            TunerModel tuner;
            tuner.bindConnection(&connection);
            TunerApplet applet(nullptr, &tuner);
            settle(applet);
            // The antenna row shows for a TGXL with the switch, reached
            // here as a station state: model fields only, no connection
            // (TunerModel.cpp:39-80).
            TunerModel::StationConnectionState station;
            station.phase = TunerModel::ConnectionPhase::Connected;
            tuner.setStationConnectionState(station);
            connection.injectLineForTesting(QStringLiteral("V1.2.17"));
            connection.injectLineForTesting(QStringLiteral("S0|state state=1 bypass=0 antA=1 3way=1"));
            for (QAbstractButton* b : applet.findChildren<QAbstractButton*>()) {
                if (b->isVisibleTo(&applet)) {
                    ++poses;
                    findings += analyseInPlace(palette, QStringLiteral("posed-TunerApplet-operate-ant2"), &applet, b);
                }
            }
        }
        {
            AmpApplet applet(nullptr);
            settle(applet);
            applet.setState(QStringLiteral("OPERATE"));
            for (QAbstractButton* b : applet.findChildren<QAbstractButton*>()) {
                if (b->isVisibleTo(&applet)) {
                    ++poses;
                    findings += analyseInPlace(palette, QStringLiteral("posed-AmpApplet-operate"), &applet, b);
                }
            }
        }
        {
            Rf2ksApplet applet(nullptr);
            settle(applet);
            applet.setOperateMode(QStringLiteral("OPERATE"));
            for (QAbstractButton* b : applet.findChildren<QAbstractButton*>()) {
                if (b->isVisibleTo(&applet)) {
                    ++poses;
                    findings += analyseInPlace(palette, QStringLiteral("posed-Rf2ksApplet-operate"), &applet, b);
                }
            }
        }
        {
            RadioModel radio;
            TxApplet tx(&radio);
            settle(tx);
            auto* psa = tx.findChild<QAbstractButton*>(QStringLiteral("TxAppletPsaBtn"));
            QVERIFY(psa != nullptr);
            psa->setVisible(true);
            {
                const QSignalBlocker block(psa);
                psa->setChecked(true);
            }
            ++poses;
            findings += analyseInPlace(palette, QStringLiteral("posed-TxApplet-psa-on"), &tx, psa);
        }
        {
            // The badge's fills by feedback level (PsaIndicatorWidget.cpp):
            // the status tones are the palette. The off grey (Thetis
            // DimGray) and the over-range blue (Thetis DodgerBlue,
            // feedback above 181) keep their colours by JJ's D32
            // (2026-10-10): neither is a status the decided tones name, and
            // the banner is read against Thetis. Both are posed, checked to
            // be those colours and counted as kept.
            PsaIndicatorWidget badge(nullptr);
            settle(badge);
            badge.setPsEnabled(false);
            ++poses;
            analyseInPlace(palette, QStringLiteral("posed-PsaIndicator-off"), &badge, &badge);
            QCOMPARE(badge.psBackgroundColor(), QColor(105, 105, 105));
            exempted[QStringLiteral("PureSignal badge off, DimGray (kept, D32)")] += 1;
            badge.setPsEnabled(true);
            badge.setMox(true);
            badge.setCorrectionsBeingApplied(true);
            for (int level : { 0, 100, 160, 200, 255 }) {
                badge.setFeedbackLevel(level);
                ++poses;
                const QVector<Finding> f = analyseInPlace(
                    palette, QStringLiteral("posed-PsaIndicator-level-%1").arg(level), &badge, &badge);
                if (level > 181) {
                    QCOMPARE(badge.psBackgroundColor(), QColor(30, 144, 255));
                    exempted[QStringLiteral("PureSignal badge over range, DodgerBlue (kept, D32)")] += 1;
                    continue;
                }
                findings += f;
            }
        }
        writeReport(findings, "NEREUS_COLOUR_SWEEP_STATES_REPORT");
        QSet<QString> colours;
        QSet<QString> owners;
        for (const Finding& f : findings) {
            colours.insert(f.colour);
            owners.insert(f.image + QLatin1Char(' ') + f.owner);
        }
        qInfo("applet states: %d poses, %d findings, %d colours, %d widgets", poses, int(findings.size()),
              int(colours.size()), int(owners.size()));
        for (auto it = exempted.cbegin(); it != exempted.cend(); ++it) {
            qInfo("applet states exempt: %s: %d", qPrintable(it.key()), it.value());
        }
        QVERIFY(poses > 0);
        QVERIFY2(findings.isEmpty(), qPrintable(describe(findings)));
    }
};

#ifdef Q_OS_LINUX
namespace {

// Subpixel (LCD) text rendering paints each glyph edge in red and blue
// steps, hundreds of colours that belong to the panel's stripe order and
// not to the app. Where the system's fontconfig asks for it (the Ubuntu
// CI runner does; Debian and the Mac's offscreen platform do not), every
// widget with text was reported. This run reads the system's
// configuration and then turns subpixel rendering off for every font, so
// text is the grey blend between its colour and the surface under it,
// which is what the palette rule checks. Set before QApplication reads
// the fonts. The file lives for the run.
struct GreyTextAntialiasing {
    QTemporaryDir dir;
    GreyTextAntialiasing()
    {
        if (!dir.isValid()) {
            return;
        }
        const QString system = (qEnvironmentVariableIsSet("FONTCONFIG_FILE")
                                    ? qEnvironmentVariable("FONTCONFIG_FILE")
                                    : QStringLiteral("/etc/fonts/fonts.conf")).toHtmlEscaped();
        QFile conf(dir.filePath(QStringLiteral("fonts.conf")));
        if (!conf.open(QIODevice::WriteOnly)) {
            return;
        }
        conf.write("<?xml version=\"1.0\"?>\n"
                   "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
                   "<fontconfig>\n"
                   "  <include ignore_missing=\"yes\">" + system.toUtf8() + "</include>\n"
                   "  <match target=\"pattern\">\n"
                   "    <edit name=\"rgba\" mode=\"assign\"><const>none</const></edit>\n"
                   "  </match>\n"
                   "  <match target=\"font\">\n"
                   "    <edit name=\"rgba\" mode=\"assign\"><const>none</const></edit>\n"
                   "  </match>\n"
                   "</fontconfig>\n");
        conf.close();
        qputenv("FONTCONFIG_FILE", conf.fileName().toLocal8Bit());
    }
};
const GreyTextAntialiasing greyTextAntialiasing;

} // namespace
#endif

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstWorkspaceColourSweep test;
    QTEST_SET_MAIN_SOURCE_PATH
    // settingsAndDialogsUseThePalette runs as entries scenes1 to scenes6, a
    // sixth of the scenes each (scenePart above); the rest as
    // tst_workspace_colour_sweep (tests/CMakeLists.txt).
    QList<NereusSDR::TestFunctionGroups::Group> groups;
    for (int k = 0; k < kSceneEntries; ++k) {
        groups.append({QStringLiteral("scenes%1").arg(k + 1),
                       {QStringLiteral("settingsAndDialogsUseThePalette")}});
    }
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), kSceneGroupVariable, groups);
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_workspace_colour_sweep.moc"
