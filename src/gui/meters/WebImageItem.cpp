// =================================================================
// src/gui/meters/WebImageItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/clsImgeFetcher.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (control ranges; no per-file header)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-26 - Thetis web image settings: interval (new items 120 s,
//                 NereusSDR keeps a 1 s minimum), width scale, bypass
//                 cache, the known-host lock and BSDWorld URL fix (from
//                 setup.cs), the fetch state (from clsImgeFetcher.cs), the
//                 presets, the 4-char ID from a guid kept in the record,
//                 aspect-kept drawing with "No Image", and file:// URLs
//                 read locally with no network request. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Background rotation per pan (Thetis clsWebImage
//                 background chain) and a guid derived from the record
//                 text for a record saved without one. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - User text in the record (URL) is joined, not substituted
//                 by chained QString::arg, so a "%1" or "%20" in it survives.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From MeterManager.cs ---
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From setup.cs ---
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From clsImgeFetcher.cs ---
/*  clsImageFetcher.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "WebImageItem.h"

// From Thetis clsWebImage (MeterManager.cs:15485+)

#include "core/mmio/FourChar.h"
#include "gui/styles/WorkspacePreview.h"

#include <QFileInfo>
#include <QFont>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcWebImageItem, "nereus.gui.meters.webimageitem")

namespace NereusSDR {

namespace {

// From Thetis setup.cs:31792 and 31795 [v2.10.3.15]
constexpr QLatin1StringView kBsdWorldHost{"bsdworld.org"};
constexpr QLatin1StringView kLightModePlaceholder{"<light_mode>"};

// Serialized tail fields (after the URL): width scale, bypass, guid; then
// target pan, background interval and next 4-char ID.
constexpr int kTailFieldCount = 3;
constexpr int kBackgroundTailFieldCount = 6;
constexpr int kFourCharLength = 4;

// NereusSDR: a record saved before the guid was kept gets one derived from
// its own text, so its 4-char ID (and a rotation naming it) is the same on
// every load until the next save writes it.
const QUuid kOldRecordGuidNamespace(QStringLiteral("6b1f4c3e-2d8a-5e90-9c47-a1d3e5f70b22"));
constexpr int kUrlField = 9;

// From Thetis MeterManager.cs:36668 [v2.10.3.15]
// (plotText("No Image", ..., System.Drawing.Color.DarkGray, ...)).
const QColor kNoImageColour(0xa9, 0xa9, 0xa9);

} // namespace

// ---------------------------------------------------------------------------
// Local serialization helpers (mirror pattern from SpacerItem.cpp)
// ---------------------------------------------------------------------------

WebImageBackgrounds& WebImageBackgrounds::instance()
{
    static WebImageBackgrounds hub;
    return hub;
}

void WebImageBackgrounds::setImage(int pan, const QImage& image)
{
    const QImage before = m_images.value(pan);
    if (before.isNull() && image.isNull()) { return; }
    if (!before.isNull() && !image.isNull() && before.cacheKey() == image.cacheKey()) { return; }
    if (image.isNull()) { m_images.remove(pan); }
    else { m_images.insert(pan, image); }
    emit panImageChanged(pan, image);
}

static QString webImageBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

static bool webImageParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

// ---------------------------------------------------------------------------
// Constructor
// From Thetis clsWebImage constructor (MeterManager.cs:15523-15561)
// ---------------------------------------------------------------------------
WebImageItem::WebImageItem(QObject* parent)
    : MeterItem(parent)
    , m_guid(QUuid::createUuid())
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &WebImageItem::onFetchFinished);

    connect(&m_refreshTimer, &QTimer::timeout,
            this, &WebImageItem::fetchImage);

    m_refreshTimer.setSingleShot(false);
    // Timer is not started until a URL is set.

    // From Thetis MeterManager.cs:15648-15670 [v2.10.3.15] (timerCallback):
    // after the background interval, the next image takes the pan.
    m_backgroundTimer.setSingleShot(true);
    connect(&m_backgroundTimer, &QTimer::timeout, this, [this]() {
        //tell the next web image to show
        showOnPan(m_targetPan, m_backgroundFourChar);
    });
    liveItems().append(this);
}

// ---------------------------------------------------------------------------
// Destructor — QObject parent ownership handles m_nam and timer.
// From Thetis MeterManager.cs:15766-15784 [v2.10.3.15] (Removing).
// ---------------------------------------------------------------------------
WebImageItem::~WebImageItem()
{
    liveItems().removeAll(this);
    // if removed, and we were being shown, then null image
    leavePan();
}

QList<WebImageItem*>& WebImageItem::liveItems()
{
    static QList<WebImageItem*> items;
    return items;
}

// From Thetis MeterManager.cs:15523-15561 [v2.10.3.15]: a new item fetches
// every 120 s at full width. Saved records keep their own interval.
void WebImageItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    m_refreshInterval = kWebImageThetisDefaults.intervalSecs;
    m_widthScale = kWebImageThetisDefaults.widthScale;
    applyHostLock();
}

// ---------------------------------------------------------------------------
// setUrl()
// From Thetis clsWebImage (MeterManager.cs:15485+)
// ---------------------------------------------------------------------------
// NereusSDR-original: previews and validation hydrate without external fetches.
void WebImageItem::setFetchEnabled(bool enabled)
{
    if (m_fetchEnabled == enabled) { return; }
    m_fetchEnabled = enabled;
    if (!enabled) {
        m_refreshTimer.stop();
        // A copy that does not fetch (a preview) never takes part in a
        // background rotation.
        liveItems().removeAll(this);
        leavePan();
    } else {
        if (!liveItems().contains(this)) { liveItems().append(this); }
        setUrl(m_url);
        backgroundDetailsUpdated();
    }
}

void WebImageItem::setTargetPan(int pan)
{
    const int clamped = (pan < 0 || pan >= kWebImageMaxPans) ? kWebImageNoPan : pan;
    if (m_targetPan == clamped) { return; }
    leavePan();
    m_targetPan = clamped;
    backgroundDetailsUpdated();
}

// From Thetis MeterManager.cs:15718-15736 [v2.10.3.15]
void WebImageItem::setBackgroundIntervalSecs(int seconds)
{
    const int clamped = std::clamp(seconds, kWebImageMinBackgroundSecs, kWebImageMaxBackgroundSecs);
    if (m_backgroundIntervalSecs == clamped) { return; }
    m_backgroundIntervalSecs = clamped;
    // Thetis compares the time shown with the interval every second
    // (timerCallback), so a new interval applies to the image now shown.
    if (m_showBackground) {
        const qint64 remaining = static_cast<qint64>(m_backgroundIntervalSecs) * 1000
                                 - m_backgroundStarted.elapsed();
        m_backgroundTimer.start(static_cast<int>(std::max<qint64>(0, remaining)));
    }
    backgroundDetailsUpdated();
}

// From Thetis MeterManager.cs:15744-15762 [v2.10.3.15]
void WebImageItem::setBackgroundFourChar(const QString& fourChar)
{
    const QString trimmed = fourChar.trimmed().left(kFourCharLength);
    if (m_backgroundFourChar == trimmed) { return; }
    m_backgroundFourChar = trimmed;
    backgroundDetailsUpdated();
}

// From Thetis MeterManager.cs:15666-15679 [v2.10.3.15] (Update): when the
// background details change, a background item starts the rotation if no
// web image is shown yet (here: on its pan).
void WebImageItem::backgroundDetailsUpdated()
{
    if (!m_fetchEnabled || m_targetPan == kWebImageNoPan) { return; }
    if (!isBackgroundShown(m_targetPan)) {
        showOnPan(m_targetPan, fourChar());
    }
}

// From Thetis MeterManager.cs:25966-25980 and 647-660 [v2.10.3.15]
// (IsWebImageBackgroundShown), for one pan.
bool WebImageItem::isBackgroundShown(int pan)
{
    if (pan == kWebImageNoPan) { return false; }
    for (const WebImageItem* item : liveItems()) {
        if (item->m_targetPan == pan && item->m_showBackground) { return true; }
    }
    return false;
}

// From Thetis MeterManager.cs:15578-15600 [v2.10.3.15]
// (OnShowWebImageBackground): the item with the 4-char ID shows and starts
// its interval; every other item stops showing and stops its timer. The
// pan's image is not cleared, so an ID that names nothing leaves the last
// image up with no rotation running, as in Thetis. NereusSDR: the turn
// stays on its pan; an ID on another pan, or on an item that is not a
// background, ends the rotation the same way.
void WebImageItem::showOnPan(int pan, const QString& fourChar)
{
    if (pan == kWebImageNoPan) { return; }
    WebImageItem* next = nullptr;
    const QList<WebImageItem*> items = liveItems();
    for (WebImageItem* item : items) {
        if (item->m_targetPan == pan && !fourChar.isEmpty() && item->fourChar() == fourChar) {
            next = item;
            break;
        }
    }
    for (WebImageItem* item : items) {
        if (item->m_targetPan == pan && item != next) { item->stopBackground(); }
    }
    if (next) { next->startBackground(); }
}

// From Thetis MeterManager.cs:15606-15644 [v2.10.3.15] (ShowBackground):
// the start time is now, and the image (when there is one) goes to the
// display.
void WebImageItem::startBackground()
{
    m_showBackground = true;
    m_backgroundStarted.start();
    m_backgroundTimer.start(m_backgroundIntervalSecs * 1000);
    imageUpdated();
}

void WebImageItem::stopBackground()
{
    m_showBackground = false;
    m_backgroundTimer.stop();
}

// NereusSDR: the item stops being its pan's background (pan changed, the
// copy stopped fetching, or it is destroyed). If it was the one shown, the
// pan gets its own background back, and another live item on the pan
// starts the rotation again so a rebuilt container keeps it running.
void WebImageItem::leavePan()
{
    const int pan = m_targetPan;
    const bool wasShown = m_showBackground;
    stopBackground();
    if (!wasShown || pan == kWebImageNoPan) { return; }
    WebImageBackgrounds::instance().setImage(pan, QImage());
    for (WebImageItem* item : liveItems()) {
        if (item != this && item->m_targetPan == pan) {
            showOnPan(pan, item->fourChar());
            break;
        }
    }
}

// A new image reaches the pan while this item is shown. NereusSDR: Thetis
// sends the image only when ShowBackground turns on (MeterManager.cs:
// 15620-15640), so an image that arrives after its turn starts waits for
// the next turn; here it goes to the pan at once.
void WebImageItem::imageUpdated()
{
    if (m_showBackground && !m_image.isNull() && m_targetPan != kWebImageNoPan) {
        WebImageBackgrounds::instance().setImage(m_targetPan, m_image);
    }
}

void WebImageItem::setUrl(const QString& url)
{
    m_url = url;
    applyHostLock();
    if (m_url.isEmpty()) {
        m_refreshTimer.stop();
        m_image = QImage();
        setState(State::Idle);
    } else if (m_fetchEnabled) {
        restartFetching();
    }
}

void WebImageItem::restartFetching()
{
    fetchImage();
    m_refreshTimer.start(m_refreshInterval * 1000);
}

// ---------------------------------------------------------------------------
// setRefreshInterval()
// ---------------------------------------------------------------------------
void WebImageItem::setRefreshInterval(int seconds)
{
    // A locked host keeps 600 s (Thetis disables the control).
    if (isLockedHost(m_url)) { return; }
    m_refreshInterval = std::clamp(seconds, kWebImageMinIntervalSecs, kWebImageMaxIntervalSecs);
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.start(m_refreshInterval * 1000);
    }
}

void WebImageItem::setWidthScale(double scale)
{
    m_widthScale = std::clamp(scale, kWebImageMinWidthScale, kWebImageMaxWidthScale);
}

void WebImageItem::setBypassCache(bool bypass)
{
    m_bypassCache = bypass && !isLockedHost(m_url);
}

QString WebImageItem::fourChar() const
{
    // From Thetis MeterManager.cs:15528 [v2.10.3.15]
    return NereusSDR::fourChar(QStringLiteral("webimage"), 0, m_guid);
}

// From Thetis setup.cs:31607-31650 [v2.10.3.15] (updateWebImageState).
QString WebImageItem::stateText() const
{
    switch (m_state) {
    case State::Idle:                   return QStringLiteral("idle");
    case State::Ok:                     return QStringLiteral("ok");
    case State::UrlIssue:               return QStringLiteral("url issue");
    case State::ImageConversionProblem: return QStringLiteral("bad image");
    case State::NoSuitableImage:        return QStringLiteral("no image");
    case State::Waiting:                return QStringLiteral("waiting");
    case State::GatheringImages:        return QStringLiteral("gathering");
    }
    return QString();
}

void WebImageItem::setState(State state)
{
    if (m_state == state) { return; }
    m_state = state;
    emit stateChanged();
}

// From Thetis setup.cs:31492-31531 [v2.10.3.15] (txtWebImage_url_TextChanged):
// the item keeps the lock so every path (editor, saved record) honours it.
void WebImageItem::applyHostLock()
{
    if (!isLockedHost(m_url)) { return; }
    // lock and set the update interval
    m_refreshInterval = kWebImageLockedHostIntervalSecs;
    // lock and set the bypass cache
    m_bypassCache = false;
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.start(m_refreshInterval * 1000);
    }
}

// From Thetis setup.cs:31494-31500 [v2.10.3.15]
bool WebImageItem::isLockedHost(const QString& url)
{
    return url.contains(QLatin1StringView("hamqsl.com"), Qt::CaseInsensitive) ||
           url.contains(QLatin1StringView("bsdworld.org"), Qt::CaseInsensitive) ||
           //txtWebImage_url.Text.Contains("nascom.nasa.gov", StringComparison.InvariantCultureIgnoreCase) ||
           //txtWebImage_url.Text.Contains("swpc.noaa.gov", StringComparison.InvariantCultureIgnoreCase) ||
           url.contains(QLatin1StringView("kc2g.com"), Qt::CaseInsensitive);
}

// From Thetis setup.cs:31788-31806 [v2.10.3.15] (fixBSDWorldUrls)
QString WebImageItem::fixBsdWorldUrl(const QString& url, bool darkMode)
{
    if (!url.contains(kBsdWorldHost, Qt::CaseInsensitive)) { return QString(); }
    if (url.contains(QLatin1StringView("-light"), Qt::CaseInsensitive)
        || url.contains(QLatin1StringView("-dark"), Qt::CaseInsensitive)) {
        return QString();
    }
    QString bare = url;
    bare.remove(kLightModePlaceholder);
    bool known = false;
    for (const WebImagePresetGroup& group : presetGroups()) {
        for (const WebImagePreset& preset : group.presets) {
            QString presetBare = preset.url;
            presetBare.remove(kLightModePlaceholder);
            if (presetBare.contains(kBsdWorldHost) && bare.contains(presetBare)) { known = true; }
        }
    }
    if (!known) { return QString(); } // the url passed is not in _bsdworld_urls

    // insert before .webp or .svgz
    const QString mode = darkMode ? QStringLiteral("-dark") : QStringLiteral("-light");
    QString fixed = url;
    if (url.indexOf(QLatin1StringView(".webp")) != -1) {
        return fixed.replace(QLatin1StringView(".webp"), mode + QStringLiteral(".webp"));
    }
    if (url.indexOf(QLatin1StringView(".svgz")) != -1) {
        return fixed.replace(QLatin1StringView(".svgz"), mode + QStringLiteral(".svgz"));
    }
    return QString();
}

// From Thetis setup.cs:31815 [v2.10.3.15]
QString WebImageItem::resolveLightMode(const QString& url, bool darkMode)
{
    QString resolved = url;
    return resolved.replace(kLightModePlaceholder,
                            darkMode ? QStringLiteral("-dark") : QStringLiteral("-light"));
}

// From Thetis setup.cs:31706-31786 [v2.10.3.15]
QList<WebImagePresetGroup> WebImageItem::presetGroups()
{
    static const QList<WebImagePresetGroup> groups{
        {QStringLiteral("HamQSL"), {
            {QStringLiteral("Layout 1 - sun"), QStringLiteral("https://www.hamqsl.com/solarn0nbh.php")},
            {QStringLiteral("Layout 2 - sun"), QStringLiteral("https://www.hamqsl.com/solarpic.php")},
            {QStringLiteral("Layout 3"), QStringLiteral("https://www.hamqsl.com/solarvhf.php")},
            {QStringLiteral("Layout 4"), QStringLiteral("https://www.hamqsl.com/solar.php")},
            {QStringLiteral("Layout 5"), QStringLiteral("https://www.hamqsl.com/solarsmall.php")},
            {QStringLiteral("Layout 6"), QStringLiteral("https://www.hamqsl.com/solarbrief.php")},
            {QStringLiteral("Layout 7"), QStringLiteral("https://www.hamqsl.com/solarbc.php")},
            {QStringLiteral("Layout 8"), QStringLiteral("https://www.hamqsl.com/solar100sc.php")},
            {QStringLiteral("Layout 9"), QStringLiteral("https://www.hamqsl.com/solar2.php")},
            {QStringLiteral("Layout 10 - sun"), QStringLiteral("https://www.hamqsl.com/solarpich.php")},
            {QStringLiteral("Layout 11 - sun"), QStringLiteral("https://www.hamqsl.com/solar101pic.php")},
            {QStringLiteral("Layout 12"), QStringLiteral("https://www.hamqsl.com/solar101vhf.php")},
            {QStringLiteral("Layout 13"), QStringLiteral("https://www.hamqsl.com/solar101vhfper.php")},
            {QStringLiteral("Layout 14 - sun"), QStringLiteral("https://www.hamqsl.com/solar101vhfpic.php")},
            {QStringLiteral("Layout 15"), QStringLiteral("https://www.hamqsl.com/solar101sc.php")},
            {QStringLiteral("Layout 16 - sun"), QStringLiteral("https://www.hamqsl.com/solarsun.php")},
            {QStringLiteral("Layout 17 - graphs"), QStringLiteral("https://www.hamqsl.com/solargraph.php")},
            {QStringLiteral("Layout 18 - graphs"), QStringLiteral("https://www.hamqsl.com/marston.php")},
            {QStringLiteral("Greyline 1"), QStringLiteral("https://www.hamqsl.com/solarmuf.php")},
            {QStringLiteral("Greyline 2"), QStringLiteral("https://www.hamqsl.com/solarmap.php")},
            {QStringLiteral("Earth 1"), QStringLiteral("https://www.hamqsl.com/solarglobe.php")},
            {QStringLiteral("Earth 2"), QStringLiteral("https://www.hamqsl.com/moonglobe.php")},
            {QStringLiteral("Planets"), QStringLiteral("https://www.hamqsl.com/solarsystem.php")},
        }},
        {QStringLiteral("BSDWorld"), {
            {QStringLiteral("NA Propagation All"), QStringLiteral("https://bsdworld.org/DXCC/continent/NA/latest<light_mode>.webp")},
            {QStringLiteral("NA Propagation Zone 3"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/3/latest<light_mode>.webp")},
            {QStringLiteral("NA Propagation Zone 4"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/4/latest<light_mode>.webp")},
            {QStringLiteral("NA Propagation Zone 5"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/5/latest<light_mode>.webp")},
            {QStringLiteral("EU Propagation All"), QStringLiteral("https://bsdworld.org/DXCC/continent/EU/tn_latest<light_mode>.webp")},
            {QStringLiteral("EU Propagation Zone 14"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/14/latest<light_mode>.webp")},
            {QStringLiteral("EU Propagation Zone 15"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/15/latest<light_mode>.webp")},
            {QStringLiteral("EU Propagation Zone 16"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/16/latest<light_mode>.webp")},
            {QStringLiteral("EU Propagation Zone 20"), QStringLiteral("https://bsdworld.org/DXCC/cqzone/20/latest<light_mode>.webp")},
            {QStringLiteral("OC Propagation All"), QStringLiteral("https://bsdworld.org/DXCC/continent/OC/tn_latest<light_mode>.webp")},
            {QStringLiteral("AS Propagation All"), QStringLiteral("https://bsdworld.org/DXCC/continent/AS/tn_latest<light_mode>.webp")},
            {QStringLiteral("SA Propagation All"), QStringLiteral("https://bsdworld.org/DXCC/continent/SA/tn_latest<light_mode>.webp")},
            {QStringLiteral("AF Propagation All"), QStringLiteral("https://bsdworld.org/DXCC/continent/AF/tn_latest<light_mode>.webp")},
            {QStringLiteral("A-Index"), QStringLiteral("https://bsdworld.org/aindex<light_mode>.svgz")},
            {QStringLiteral("PK Index"), QStringLiteral("https://bsdworld.org/pkindex<light_mode>.svgz")},
            {QStringLiteral("PK Predictions"), QStringLiteral("https://bsdworld.org/pki-forecast<light_mode>.svgz")},
            {QStringLiteral("Flux"), QStringLiteral("https://bsdworld.org/flux<light_mode>.svgz")},
            {QStringLiteral("Outlook"), QStringLiteral("https://bsdworld.org/outlook<light_mode>.svgz")},
            {QStringLiteral("Solar Wind"), QStringLiteral("https://bsdworld.org/solarwind<light_mode>.svgz")},
            {QStringLiteral("SSN"), QStringLiteral("https://bsdworld.org/ssn<light_mode>.svgz")},
            {QStringLiteral("SSN History"), QStringLiteral("https://bsdworld.org/ssnhist<light_mode>.svgz")},
            {QStringLiteral("EISN"), QStringLiteral("https://bsdworld.org/eisn<light_mode>.svgz")},
            {QStringLiteral("Proton Flux"), QStringLiteral("https://bsdworld.org/proton_flux<light_mode>.svgz")},
            {QStringLiteral("X-Ray Flux"), QStringLiteral("https://bsdworld.org/xray_flux<light_mode>.svgz")},
            {QStringLiteral("D-Layer"), QStringLiteral("https://bsdworld.org/d-rap/latest<light_mode>.svgz")},
        }},
        {QStringLiteral("NASA"), {
            {QStringLiteral("SOHO EIT 171"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/eit_171/512/latest.jpg")},
            {QStringLiteral("SOHO EIT 195"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/eit_195/512/latest.jpg")},
            {QStringLiteral("SOHO EIT 284"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/eit_284/512/latest.jpg")},
            {QStringLiteral("SOHO EIT 304"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/eit_304/512/latest.jpg")},
            {QStringLiteral("SOHO SDO/HMI Continuum"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/hmi_igr/512/latest.jpg")},
            {QStringLiteral("SOHO SDO/HMI Magnetogram"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/hmi_mag/512/latest.jpg")},
            {QStringLiteral("SOHO LASCO C2"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/c2/512/latest.jpg")},
            {QStringLiteral("SOHO LASCO C3"), QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/c3/512/latest.jpg")},
        }},
        {QStringLiteral("NOAA"), {
            {QStringLiteral("Northern Aurora Latest"), QStringLiteral("https://services.swpc.noaa.gov/images/animations/ovation/north/latest.jpg")},
            {QStringLiteral("Southern Aurora Latest"), QStringLiteral("https://services.swpc.noaa.gov/images/animations/ovation/south/latest.jpg")},
            {QStringLiteral("Northern Aurora Forecast"), QStringLiteral("https://services.swpc.noaa.gov/images/aurora-forecast-northern-hemisphere.jpg")},
            {QStringLiteral("Southern Aurora Forecast"), QStringLiteral("https://services.swpc.noaa.gov/images/aurora-forecast-southern-hemisphere.jpg")},
            {QStringLiteral("SWX Solar Overiew"), QStringLiteral("https://services.swpc.noaa.gov/images/swx-overview-large.gif")},
            {QStringLiteral("K Indicies"), QStringLiteral("https://services.swpc.noaa.gov/images/station-k-index.png")},
            {QStringLiteral("D Region Absorption Map"), QStringLiteral("https://services.swpc.noaa.gov/images/animations/d-rap/global/d-rap/latest.png")},
        }},
    };
    return groups;
}

// From Thetis MeterManager.cs:15878-15909 [v2.10.3.15]: a file:// URL is
// read from its local path (ImageFetcher.RegisterURL(..., isFile, ...)).
bool WebImageItem::isFileUrl() const
{
    return m_url.startsWith(QLatin1StringView("file://"), Qt::CaseInsensitive);
}

void WebImageItem::loadLocalFile()
{
    const QString path = QUrl(m_url).toLocalFile();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        setState(State::UrlIssue);
        return;
    }
    QImage img(path);
    if (img.isNull()) {
        qCWarning(lcWebImageItem) << "Could not decode image file" << path;
        setState(State::ImageConversionProblem);
        return;
    }
    m_image = img;
    setState(State::Ok);
    imageUpdated();
}

// ---------------------------------------------------------------------------
// fetchImage()
// From Thetis clsImgeFetcher.cs:218-454 [v2.10.3.15] (fetch_images)
// ---------------------------------------------------------------------------
void WebImageItem::fetchImage()
{
    if (!m_fetchEnabled) { return; }
    if (m_url.isEmpty() || m_fetchInProgress) {
        return;
    }
    if (isFileUrl()) {
        loadLocalFile();
        return;
    }
    QUrl url(m_url);
    // From Thetis clsImgeFetcher.cs:235-242 [v2.10.3.15]
    if (m_bypassCache) // essentially build a unique url each time
    {
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("nereus_id"),
                           QUuid::createUuid().toString(QUuid::WithoutBraces));
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    // From Thetis clsImgeFetcher.cs:248 [v2.10.3.15] (NoCacheNoStore).
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    m_fetchInProgress = true;
    ++m_networkRequestCount;
    setState(State::GatheringImages);
    m_nam->get(request);
}

// ---------------------------------------------------------------------------
// onFetchFinished()
// From Thetis clsWebImage — handle HTTP reply, update image
// ---------------------------------------------------------------------------
void WebImageItem::onFetchFinished(QNetworkReply* reply)
{
    m_fetchInProgress = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcWebImageItem) << "Failed to fetch image from" << m_url
                                  << ":" << reply->errorString();
        setState(State::UrlIssue);
        return; // Keep the existing image on failure.
    }

    QImage img;
    img.loadFromData(reply->readAll());
    if (img.isNull()) {
        qCWarning(lcWebImageItem) << "Could not decode image data from" << m_url;
        setState(State::ImageConversionProblem);
        return;
    }

    m_image = img;
    setState(State::Ok);
    imageUpdated();
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis renderWebImage (MeterManager.cs:36578-36670 [v2.10.3.15])
// ---------------------------------------------------------------------------
void WebImageItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect itemRect = pixelRect(widgetW, widgetH);
    p.fillRect(itemRect, m_fallbackColor);
    if (m_image.isNull()) {
        // From Thetis MeterManager.cs:36668 [v2.10.3.15]
        p.save();
        QFont font = WorkspacePreview::readableFont(p.font(), p.device());
        font.setPixelSize(std::max(8, std::min(itemRect.height() / 4, itemRect.width() / 8)));
        p.setFont(font);
        p.setPen(kNoImageColour);
        p.drawText(itemRect, Qt::AlignCenter, QStringLiteral("No Image"));
        p.restore();
        return;
    }

    // From Thetis MeterManager.cs:27262-27264 [v2.10.3.15]: the image is
    // WidthScale wide, centred (TopLeft.X = 0.5 - scale / 2).
    const double w = itemRect.width() * m_widthScale;
    const double h = itemRect.height();
    const double x = itemRect.left() + (itemRect.width() - w) / 2.0;
    const double y = itemRect.top();
    QRectF imgRect(x, y, w, h);

    // From Thetis MeterManager.cs:36654-36662 [v2.10.3.15]
    // maintain aspect ratio, the clip removes anything outside the rect
    // Thetis sizes the item to the image (Size.Height = width * image
    // height / width, MeterManager.cs:15965-15974), so both of its branches
    // draw the image at the full scaled width. A NereusSDR item keeps the
    // height the user gave it, so the image is drawn at that width with
    // its own height, from the top; the clip trims an image taller than
    // the item.
    const double imW = m_image.width();
    const double imH = m_image.height();
    imgRect.setHeight(imgRect.width() * (imH / imW));

    p.save();
    // The clip is the image's own (width-scaled) rect, as Thetis clips to
    // the web image item.
    p.setClipRect(QRectF(x, y, w, h));
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(imgRect, m_image);
    p.restore();
}

// ---------------------------------------------------------------------------
// serialize()
// Format: WEBIMAGE|x|y|w|h|bindingId|zOrder|refreshInterval|fallbackColor|url
//         |widthScale|bypassCache|guid|targetPan|backgroundSecs|nextFourChar
//         [0]      [1-6]             [7]            [8]           [9]
//         [10]       [11]        [12]  [13]      [14]           [15]
//         (10-12 were added later; a record without them loads with
//         scale 1, bypass off and a guid derived from its text. 13-15
//         came after them; a record without them is no background.)
// ---------------------------------------------------------------------------
QString WebImageItem::serialize() const
{
    // Joined, not chained .arg(): a URL holding "%2F" must stay as written.
    return QStringList{
        QStringLiteral("WEBIMAGE"),
        webImageBaseFields(*this),
        QString::number(m_refreshInterval),
        m_fallbackColor.name(QColor::HexArgb),
        m_url,
        QString::number(m_widthScale),
        m_bypassCache ? QStringLiteral("1") : QStringLiteral("0"),
        m_guid.toString(QUuid::WithoutBraces),
        QString::number(m_targetPan),
        QString::number(m_backgroundIntervalSecs),
        m_backgroundFourChar}.join(QLatin1Char('|'));
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected: WEBIMAGE|x|y|w|h|bindingId|zOrder|refreshInterval|fallbackColor|url
//           [|widthScale|bypassCache|guid[|targetPan|backgroundSecs|nextFourChar]]
// ---------------------------------------------------------------------------
bool WebImageItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < kUrlField + 1 || parts[0] != QLatin1String("WEBIMAGE")) {
        return false;
    }
    if (!webImageParseBaseFields(*this, parts)) {
        return false;
    }

    bool ok = true;
    const int refreshInterval = parts[7].toInt(&ok);
    if (!ok) {
        return false;
    }

    const QColor fallbackColor(parts[8]);
    if (!fallbackColor.isValid()) {
        return false;
    }

    // The tail (width scale, bypass, guid) is read from the end so a URL
    // holding '|' stays whole; a record without a valid tail is the older
    // form whose URL is field 9 alone.
    QString url = parts[kUrlField];
    double widthScale = kWebImageThetisDefaults.widthScale;
    bool bypass = false;
    QUuid guid;
    int targetPan = kWebImageNoPan;
    int backgroundSecs = kWebImageThetisDefaults.backgroundIntervalSecs;
    QString nextFourChar;
    int n = static_cast<int>(parts.size());
    // The background fields come last; a record with them ends in a pan
    // number, an interval and a short (possibly empty) ID.
    if (n >= kUrlField + 1 + kBackgroundTailFieldCount) {
        bool panOk = false;
        bool secsOk = false;
        const int pan = parts[n - 3].toInt(&panOk);
        const int secs = parts[n - 2].toInt(&secsOk);
        const QString& next = parts[n - 1];
        const QUuid tailGuid(parts[n - 4]);
        if (panOk && secsOk && next.size() <= kFourCharLength && !tailGuid.isNull()) {
            targetPan = pan;
            backgroundSecs = secs;
            nextFourChar = next;
            n -= kBackgroundTailFieldCount - kTailFieldCount;
        }
    }
    if (n >= kUrlField + 1 + kTailFieldCount) {
        bool scaleOk = false;
        const double scale = parts[n - 3].toDouble(&scaleOk);
        const QString& bypassField = parts[n - 2];
        const QUuid tailGuid(parts[n - 1]);
        const bool bypassOk = bypassField == QLatin1String("0") || bypassField == QLatin1String("1");
        if (scaleOk && bypassOk && !tailGuid.isNull()) {
            widthScale = scale;
            bypass = bypassField == QLatin1String("1");
            guid = tailGuid;
            url = QStringList(parts.mid(kUrlField, n - kTailFieldCount - kUrlField)).join(QLatin1Char('|'));
        }
    }

    m_refreshInterval = std::clamp(refreshInterval, kWebImageMinIntervalSecs, kWebImageMaxIntervalSecs);
    m_fallbackColor = fallbackColor;
    m_widthScale = std::clamp(widthScale, kWebImageMinWidthScale, kWebImageMaxWidthScale);
    m_bypassCache = bypass;
    if (guid.isNull()) {
        guid = QUuid::createUuidV5(kOldRecordGuidNamespace, data.toUtf8());
    }
    setGuid(guid);
    // Use setUrl() so the host lock, timer and fetch are applied correctly.
    setUrl(url);
    m_backgroundIntervalSecs = std::clamp(backgroundSecs, kWebImageMinBackgroundSecs,
                                          kWebImageMaxBackgroundSecs);
    m_backgroundFourChar = nextFourChar;
    if (targetPan < 0 || targetPan >= kWebImageMaxPans) { targetPan = kWebImageNoPan; }
    if (targetPan != m_targetPan) {
        leavePan();
        m_targetPan = targetPan;
    }
    backgroundDetailsUpdated();

    return true;
}

} // namespace NereusSDR
