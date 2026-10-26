#pragma once

// =================================================================
// src/gui/meters/WebImageItem.h  (NereusSDR)
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
//                 cache, the known-host lock and BSDWorld URL fix, the
//                 fetch state, the presets, the 4-char ID from a guid
//                 kept in the record, aspect-kept drawing with "No Image",
//                 and file:// URLs read locally. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - A web image can be a pan's spectrum background, and
//                 background images rotate per pan: each shows for its
//                 background interval (5 s default) and then hands the
//                 pan to the next item named by its 4-char ID (Thetis
//                 clsWebImage background chain). A record saved without
//                 a guid gets one derived from its text, so its 4-char
//                 ID is the same on every load. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

#include "MeterItem.h"
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QList>
#include <QPair>
#include <QTimer>
#include <QUuid>

class QNetworkAccessManager;
class QNetworkReply;

namespace NereusSDR {

// From Thetis MeterManager.cs:15523-15561 [v2.10.3.15] (clsWebImage
// constructor: _secs_interval = 120; _background_interval = 5;
// _width_scale = 1f;). Values a new web image item takes.
struct WebImageDefaults {
    int    intervalSecs;
    int    backgroundIntervalSecs;
    double widthScale;
};
inline constexpr WebImageDefaults kWebImageThetisDefaults{120, 5, 1.0};

// From Thetis setup.designer.cs:62677-62686 [v2.10.3.15]
// (nudWebImage_update_interval.Minimum = 30, Maximum = 7200).
constexpr int kWebImageThetisMinIntervalSecs = 30;
constexpr int kWebImageMaxIntervalSecs = 7200;
// NereusSDR option: refreshes down to 1 s stay allowed (the editor's
// minimum before the Thetis settings were ported).
constexpr int kWebImageMinIntervalSecs = 1;
// From Thetis setup.cs:31514 [v2.10.3.15]
// (nudWebImage_update_interval.Value = (decimal)600;) for a locked host.
constexpr int kWebImageLockedHostIntervalSecs = 600;
// From Thetis setup.designer.cs:62701-62727 [v2.10.3.15]
// (nudWebImage_width_scale: DecimalPlaces 3, Increment and Minimum 0.001,
// Maximum 1, Value 1; "Width scale. 1.0 will fill the container width").
constexpr double kWebImageMinWidthScale = 0.001;
constexpr double kWebImageMaxWidthScale = 1.0;
constexpr double kWebImageWidthScaleStep = 0.001;
constexpr int    kWebImageWidthScaleDecimals = 3;

// A named URL from one of the Thetis preset lists.
struct WebImagePreset {
    QString name;
    QString url;
};
struct WebImagePresetGroup {
    QString name;
    QList<WebImagePreset> presets;
};

// From Thetis clsWebImage (MeterManager.cs:15485+)
// Displays an image fetched from a URL with periodic refresh.
// From Thetis MeterManager.cs:27259 [v2.10.3.15]
// (webimage_background_interval: 5..3600 s, default 5) and the setup
// page's nudWebImage_background_time (setup.cs:25946).
constexpr int kWebImageMinBackgroundSecs = 5;
constexpr int kWebImageMaxBackgroundSecs = 3600;

// No pan: the item is not a background (Thetis Background off).
constexpr int kWebImageNoPan = -1;
// Pans a web image can name: the app shows at most eight
// (DisplayLoadGovernor::kCeilingPans).
constexpr int kWebImageMaxPans = 8;

// NereusSDR: what each pan's spectrum background shows from a web image
// (Thetis has one display and sets console.PnlDisplayBackgroundImage,
// MeterManager.cs:15620-15640). A null image means the pan's own
// background. Only live items write here; previews never do.
class WebImageBackgrounds : public QObject {
    Q_OBJECT
public:
    static WebImageBackgrounds& instance();
    QImage image(int pan) const { return m_images.value(pan); }
    void setImage(int pan, const QImage& image);

signals:
    void panImageChanged(int pan, const QImage& image);

private:
    WebImageBackgrounds() = default;
    QHash<int, QImage> m_images;
};

class WebImageItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis clsImgeFetcher.cs:60-69 [v2.10.3.15] ImageFetcher.State
    // (OK = 0 ... GATHERING_IMAGES, IDLE = 99).
    enum class State {
        Ok = 0,
        NoSuitableImage,
        UrlIssue,
        ImageConversionProblem,
        Waiting,
        GatheringImages,
        Idle = 99
    };

    explicit WebImageItem(QObject* parent = nullptr);
    ~WebImageItem() override;

    void setFetchEnabled(bool enabled);
    bool fetchEnabled() const { return m_fetchEnabled; }

    // A known host (hamqsl.com, bsdworld.org, kc2g.com) forces the
    // interval to 600 s and turns bypass cache off while it is the URL.
    void setUrl(const QString& url);
    QString url() const { return m_url; }

    // Seconds, kept within kWebImageMinIntervalSecs..kWebImageMaxIntervalSecs;
    // ignored while the URL is on a locked host.
    void setRefreshInterval(int seconds);
    int refreshInterval() const { return m_refreshInterval; }

    // Fraction of the item width the image fills, centred (Thetis
    // WidthScale, kWebImageMinWidthScale..kWebImageMaxWidthScale).
    void setWidthScale(double scale);
    double widthScale() const { return m_widthScale; }

    // Each request gets a unique query parameter; forced off on a locked host.
    void setBypassCache(bool bypass);
    bool bypassCache() const { return m_bypassCache; }

    void setFallbackColor(const QColor& c) { m_fallbackColor = c; }
    QColor fallbackColor() const { return m_fallbackColor; }

    // The guid kept in the record; fourChar() is derived from it.
    QUuid guid() const { return m_guid; }
    void setGuid(const QUuid& guid) { if (!guid.isNull()) { m_guid = guid; } }
    // From Thetis MeterManager.cs:15528 [v2.10.3.15]
    // (_four_char = Common.FourChar("webimage", 0, guid);)
    QString fourChar() const;

    QImage currentImage() const { return m_image; }

    // The pan whose spectrum background this item supplies, 0-based, or
    // kWebImageNoPan (Thetis Background, MeterManager.cs:15707-15716).
    void setTargetPan(int pan);
    int targetPan() const { return m_targetPan; }
    // Seconds this image stays on the pan before the next one takes over,
    // kWebImageMinBackgroundSecs..kWebImageMaxBackgroundSecs (Thetis
    // BackgroundInterval, MeterManager.cs:15718-15736).
    void setBackgroundIntervalSecs(int seconds);
    int backgroundIntervalSecs() const { return m_backgroundIntervalSecs; }
    // The 4-char ID of the web image that follows this one on the pan;
    // empty ends the rotation (Thetis BackgroundFourChar,
    // MeterManager.cs:15744-15762).
    void setBackgroundFourChar(const QString& fourChar);
    QString backgroundFourChar() const { return m_backgroundFourChar; }
    // Whether this item's image is the one its pan shows now (Thetis
    // ShowBackground). Only a live item (fetch enabled) ever shows.
    bool showsBackground() const { return m_showBackground; }

    // Whether a live web image is shown on the pan (Thetis
    // IsWebImageBackgroundShown, MeterManager.cs:647-660, per pan here).
    static bool isBackgroundShown(int pan);
    // Hands the pan to the live item on it whose 4-char ID is fourChar;
    // every other item on the pan stops showing. An ID that names no item
    // on the pan stops the rotation and the pan keeps the last image
    // (Thetis ShowWebImageBackground, MeterManager.cs:15578-15600).
    static void showOnPan(int pan, const QString& fourChar);
    State state() const { return m_state; }
    // Thetis lblWebImage_state text for state() (setup.cs:31607-31650).
    QString stateText() const;

    // Network requests made so far; a file:// URL makes none.
    int networkRequestCount() const { return m_networkRequestCount; }

    // From Thetis setup.cs:31492-31500 [v2.10.3.15]: URLs on these hosts
    // lock the interval to 600 s and bypass cache off.
    static bool isLockedHost(const QString& url);
    // From Thetis setup.cs:31788-31806 [v2.10.3.15] (fixBSDWorldUrls): an
    // old BSDWorld preset URL with no -light/-dark gets one inserted before
    // .webp or .svgz; returns an empty string when there is nothing to fix.
    static QString fixBsdWorldUrl(const QString& url, bool darkMode);
    // From Thetis setup.cs:31706-31786 [v2.10.3.15] (_hamqsl_urls,
    // _bsdworld_urls, _nasa_urls, _noaa_urls without their "select one"
    // rows). BSDWorld URLs keep the <light_mode> placeholder.
    static QList<WebImagePresetGroup> presetGroups();
    // A BSDWorld preset URL with <light_mode> set to -dark or -light
    // (setup.cs:31815).
    static QString resolveLightMode(const QString& url, bool darkMode);

    void applyNewItemDefaults() override;

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void stateChanged();

private slots:
    void fetchImage();
    void onFetchFinished(QNetworkReply* reply);

private:
    void setState(State state);
    void imageUpdated();
    // Rotation (live items only).
    void backgroundDetailsUpdated();
    void startBackground();
    void stopBackground();
    void leavePan();
    static QList<WebImageItem*>& liveItems();
    void applyHostLock();
    void restartFetching();
    bool isFileUrl() const;
    void loadLocalFile();

    QString m_url;
    int     m_refreshInterval{300}; // seconds; saved records keep theirs
    double  m_widthScale{kWebImageThetisDefaults.widthScale};
    bool    m_bypassCache{false};
    QColor  m_fallbackColor{0x20, 0x20, 0x20};
    QImage  m_image;
    QUuid   m_guid;
    State   m_state{State::Idle};
    bool    m_fetchEnabled{true};
    bool    m_fetchInProgress{false};
    int     m_networkRequestCount{0};
    int     m_targetPan{kWebImageNoPan};
    int     m_backgroundIntervalSecs{kWebImageThetisDefaults.backgroundIntervalSecs};
    QString m_backgroundFourChar;
    bool    m_showBackground{false};

    QNetworkAccessManager* m_nam{nullptr};
    QTimer m_refreshTimer;
    QTimer m_backgroundTimer;
    QElapsedTimer m_backgroundStarted;
};

} // namespace NereusSDR
