// =================================================================
// src/gui/hotkeys/devices/DeviceOutput.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - TMate 2 frame and Stream Deck+ packets built on AetherSDR
//                 src/core/HidEncoderManager.cpp:13-38,105-136,282-294,
//                 299-314,535-546,644-761 and
//                 src/gui/MainWindow_Controllers.cpp:1111-1197 [@1e0718ad] for
//                 NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code (Claude Opus 5.5).
//                 Hot keys plan Task 14. Builds bytes for the USB worker to
//                 send, where upstream writes to the device as it builds them.
//                 The main digits and three fixed segments only. Key and strip
//                 images are drawn for this design.
//   2026-11-06 - The off state for hot keys plan Task 17, J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code
//                 (Claude Opus 5.5).
// =================================================================

#include "gui/hotkeys/devices/DeviceOutput.h"

#include "core/LogCategories.h"

#include <QBuffer>
#include <QFont>
#include <QPainter>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <cstring>

namespace NereusSDR::DeviceOutput {

namespace {

// From AetherSDR src/core/HidEncoderManager.cpp:13-22 [@1e0718ad]
// Digit encoding for the TMate 2 LCD.  Tables and byte layout are derived
// from OpenTMate2Lib (D:\Code\OpenTMate2Lib), which reverse-engineered the
// protocol from TMATE2_DLL.dll via USBPcap captures (2026-06-05).
//
// Main 9-digit display (freq in Hz):
//   Each digit d (1=units, 9=100 MHz) occupies two LCDVector bytes:
//     high = 22 - 2*d   bits: A=0x01 B=0x02 C=0x04
//     low  = 21 - 2*d   bits: F=0x01 G=0x02 E=0x04 D=0x08

// From AetherSDR src/core/HidEncoderManager.cpp:33-38 [@1e0718ad]
constexpr quint8 kMainHigh[10] = {
    0x07, 0x06, 0x03, 0x07, 0x06, 0x05, 0x05, 0x07, 0x07, 0x07
};
constexpr quint8 kMainLow[10] = {
    0x0D, 0x00, 0x0E, 0x0A, 0x03, 0x0B, 0x0F, 0x00, 0x0F, 0x0B
};

// From AetherSDR src/core/HidEncoderManager.cpp:105-107,134-136 [@1e0718ad]
// Indicator segment lookup: {byte_index, bitmask}.
// Derived from USB captures (session_20260605_051711/segments_*.csv).
// Byte range 0-31 only; indicator bits never overlap with digit A-G bits.
struct SegEntry {
    int byte;
    quint8 mask;
};
constexpr SegEntry kSegDot1 = {9, 0x10};    // after digit 3 (kHz/Hz)
constexpr SegEntry kSegDot2 = {15, 0x10};   // after digit 6 (MHz/kHz)
constexpr SegEntry kSegHz = {23, 0x01};

// LCDVector byte offsets (matches OpenTMate2Lib and Delphi LCD_SEGMENT_DEF)
// From AetherSDR src/core/HidEncoderManager.cpp:282-294 [@1e0718ad]
constexpr int kTm2Led = 32;
constexpr int kTm2BacklightR = 33;
constexpr int kTm2BacklightG = 34;
constexpr int kTm2BacklightB = 35;
constexpr int kTm2Contrast = 36;
constexpr int kTm2Refresh = 37;
constexpr int kTm2Speed1 = 38;
constexpr int kTm2Speed2 = 39;
constexpr int kTm2Speed3 = 40;
constexpr int kTm2Thr12 = 41;
constexpr int kTm2Thr23 = 42;
constexpr int kTm2Eval = 43;

// hidapi write buffers include a leading report ID byte.  The TMate 2 OUT
// payload captured on USB is the 64-byte LCDVector report itself, so prepend
// report ID 0 to avoid shifting the vector left by one byte on Windows.
// [original inline comment from src/core/HidEncoderManager.cpp:752-754]
constexpr int kTm2FrameSize = 65;

// From AetherSDR src/core/HidEncoderManager.h:93-95 [@1e0718ad]
constexpr quint8 kLedConnected = 0x01;
constexpr quint8 kLedLocked = 0x02;

// From AetherSDR src/core/HidEncoderManager.cpp:535-546 [@1e0718ad]
constexpr quint8 kBacklightR = 0;
constexpr quint8 kBacklightG = 50;
constexpr quint8 kBacklightB = 255;
constexpr quint64 kMaxHz = 999999999U;

// From AetherSDR src/core/HidEncoderManager.cpp:650-652,699-701 [@1e0718ad]
constexpr int kKeyHeaderSize = 8;
constexpr int kStripHeaderSize = 16;
constexpr quint8 kReportId = 0x02;
constexpr quint8 kCommandKeyImage = 0x07;
constexpr quint8 kCommandStripImage = 0x0C;

// From AetherSDR src/gui/MainWindow_Controllers.cpp:1179,1196 [@1e0718ad]
constexpr int kKeySize = 120;
constexpr int kKeyJpegQuality = 90;
// From AetherSDR src/gui/MainWindow_Controllers.cpp:1117,1172 [@1e0718ad]
constexpr int kStripWidth = 800;
constexpr int kStripHeight = 100;
constexpr int kStripColumns = 4;
constexpr int kStripJpegQuality = 92;

void setTm2FixedFields(quint8* vector)
{
    // Matches the original Delphi app / TMate2Probe captures. 0x28 here
    // overdrives the LCD on some units and can make the display unreadable.
    // [original inline comment from src/core/HidEncoderManager.cpp:533-534]
    vector[kTm2Contrast] = 0x00;
    vector[kTm2Refresh] = 0x28;
    vector[kTm2Speed1] = 0x01;
    vector[kTm2Speed2] = 0x05;
    vector[kTm2Speed3] = 0x0A;
    vector[kTm2Thr12] = 0x0F;
    vector[kTm2Thr23] = 0x19;
    vector[kTm2Eval] = 0x0A;
}

void setSegment(quint8* vector, SegEntry segment)
{
    vector[segment.byte] |= segment.mask;
}

void put16(char* at, int value)
{
    at[0] = static_cast<char>(value & 0xFF);
    at[1] = static_cast<char>((value >> 8) & 0xFF);
}

QByteArray encodeJpeg(const QImage& image, int quality)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", quality)) {
        qCWarning(lcHotKeys) << "Couldn't make a JPEG for a Stream Deck+ image";
        return {};
    }
    return bytes;
}

template <typename WriteHeader>
QList<QByteArray> split(const QByteArray& jpeg, int headerSize, WriteHeader writeHeader)
{
    const int payloadSize = kStreamDeckPacketSize - headerSize;
    QList<QByteArray> packets;
    int offset = 0;
    int pageNumber = 0;
    while (offset < jpeg.size()) {
        const int chunkLen = std::min(payloadSize, static_cast<int>(jpeg.size()) - offset);
        const bool isLast = offset + chunkLen >= jpeg.size();
        QByteArray packet(kStreamDeckPacketSize, '\0');
        writeHeader(packet.data(), chunkLen, isLast, pageNumber);
        std::memcpy(packet.data() + headerSize, jpeg.constData() + offset, static_cast<size_t>(chunkLen));
        packets.append(packet);
        offset += chunkLen;
        ++pageNumber;
    }
    return packets;
}

} // namespace

QByteArray tmate2OffFrame()
{
    QByteArray frame(kTm2FrameSize, '\0');
    // Backlight 0, 0, 0 as upstream's clean close
    // (AetherSDR src/core/HidEncoderManager.cpp:567-570 [@1e0718ad]). No segment and no LED.
    setTm2FixedFields(reinterpret_cast<quint8*>(frame.data()) + 1);
    return frame;
}

QByteArray tmate2Frame(const QString& frequency, bool connected, bool locked)
{
    QByteArray frame(kTm2FrameSize, '\0');
    auto* const vector = reinterpret_cast<quint8*>(frame.data()) + 1;

    setTm2FixedFields(vector);
    vector[kTm2BacklightR] = kBacklightR;
    vector[kTm2BacklightG] = kBacklightG;
    vector[kTm2BacklightB] = kBacklightB;
    vector[kTm2Led] = static_cast<quint8>((connected ? kLedConnected : 0) | (locked ? kLedLocked : 0));

    // Always-on static indicators for a frequency display
    // [original inline comment from src/core/HidEncoderManager.cpp:547]
    setSegment(vector, kSegDot1);
    setSegment(vector, kSegDot2);
    setSegment(vector, kSegHz);

    quint64 hz = 0;
    bool hasDigits = false;
    for (const QChar ch : frequency) {
        if (ch.isDigit()) {
            hasDigits = true;
            hz = std::min(hz * 10 + static_cast<quint64>(ch.digitValue()), kMaxHz);
        }
    }
    if (!hasDigits) {
        return frame;
    }
    // From AetherSDR src/core/HidEncoderManager.cpp:299-314 [@1e0718ad]
    for (int d = 1; d <= 9; ++d) {
        if (hz == 0U && d > 1) {
            continue;
        }
        vector[22 - 2 * d] |= kMainHigh[hz % 10U];
        vector[21 - 2 * d] |= kMainLow[hz % 10U];
        hz /= 10U;
    }
    return frame;
}

QList<QByteArray> streamDeckKeyPackets(int key, const QByteArray& jpeg)
{
    // StreamDeck+ LCD image write: 1024-byte feature reports (report ID 0x02),
    // command 0x07 (set key image). Protocol verified against python-elgato-streamdeck.
    // [original inline comment from src/core/HidEncoderManager.cpp:648-649]
    return split(jpeg, kKeyHeaderSize, [key](char* packet, int chunkLen, bool isLast, int pageNumber) {
        packet[0] = static_cast<char>(kReportId);   // report ID
        packet[1] = static_cast<char>(kCommandKeyImage);    // command: set key image
        packet[2] = static_cast<char>(key);
        packet[3] = isLast ? 1 : 0;
        put16(packet + 4, chunkLen);
        put16(packet + 6, pageNumber);
    });
}

QList<QByteArray> streamDeckStripPackets(const QByteArray& jpeg)
{
    // Touchscreen write: 1024-byte packets, 16-byte header, command 0x0c.
    // Protocol verified against python-elgato-streamdeck StreamDeckPlus.set_touchscreen_image().
    // [original inline comment from src/core/HidEncoderManager.cpp:697-698]
    return split(jpeg, kStripHeaderSize, [](char* packet, int chunkLen, bool isLast, int pageNumber) {
        packet[0] = static_cast<char>(kReportId);
        packet[1] = static_cast<char>(kCommandStripImage);
        put16(packet + 2, 0);
        put16(packet + 4, 0);
        put16(packet + 6, kStripWidth);
        put16(packet + 8, kStripHeight);
        packet[10] = isLast ? 1 : 0;
        put16(packet + 11, pageNumber);
        put16(packet + 13, chunkLen);
        packet[15] = 0x00;
    });
}

QList<QByteArray> streamDeckKeyImage(int key, const QImage& image120)
{
    const QImage image = image120.size() == QSize(kKeySize, kKeySize)
        ? image120
        : image120.scaled(kKeySize, kKeySize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return streamDeckKeyPackets(key, encodeJpeg(image.convertToFormat(QImage::Format_RGB32), kKeyJpegQuality));
}

QList<QByteArray> streamDeckStrip(const QImage& image800x100)
{
    const QImage image = image800x100.size() == QSize(kStripWidth, kStripHeight)
        ? image800x100
        : image800x100.scaled(kStripWidth, kStripHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return streamDeckStripPackets(encodeJpeg(image.convertToFormat(QImage::Format_RGB32), kStripJpegQuality));
}

QImage renderStreamDeckKey(const QString& label, const QColor& background)
{
    // From AetherSDR src/gui/MainWindow_Controllers.cpp:1179-1191 [@1e0718ad]
    QImage image(kKeySize, kKeySize, QImage::Format_RGB32);
    image.fill(background);
    if (!label.isEmpty()) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing);
        QFont font;
        font.setPixelSize(label.length() > 6 ? 22 : 28);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRect(4, 4, 112, 112), Qt::AlignCenter | Qt::TextWordWrap, label);
    }
    return image;
}

QImage renderStreamDeckBlankStrip()
{
    QImage image(kStripWidth, kStripHeight, QImage::Format_RGB32);
    image.fill(Qt::black);
    return image;
}

QImage renderStreamDeckStrip(const QString& slice, const QString& frequency, const QString& mode, const QString& filter)
{
    // One column over each dial, as upstream: the palette and the two halves are from
    // AetherSDR src/gui/MainWindow_Controllers.cpp:1119-1163 [@1e0718ad]. What the
    // columns say is this design's: the feed's display, not each dial's action.
    constexpr int kColumnWidth = kStripWidth / kStripColumns;
    constexpr int kLabelHeight = 34;
    const QString labels[kStripColumns] = {QStringLiteral("Slice"), QStringLiteral("Frequency"),
                                           QStringLiteral("Mode"), QStringLiteral("Filter")};
    const QString values[kStripColumns] = {slice, frequency, mode, filter};

    QImage image(kStripWidth, kStripHeight, QImage::Format_RGB32);
    image.fill(QColor(15, 15, 20));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    for (int i = 0; i < kStripColumns; ++i) {
        const int x = i * kColumnWidth;
        painter.fillRect(x, 0, kColumnWidth - 1, kLabelHeight, QColor(18, 28, 52));
        painter.fillRect(x + kColumnWidth - 1, 0, 1, kStripHeight, QColor(40, 40, 50));

        QFont labelFont;
        labelFont.setPixelSize(16);
        painter.setFont(labelFont);
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(QRect(x + 2, 0, kColumnWidth - 4, kLabelHeight), Qt::AlignCenter, labels[i]);

        QFont valueFont;
        valueFont.setPixelSize(28);
        valueFont.setBold(true);
        painter.setFont(valueFont);
        painter.setPen(Qt::white);
        painter.drawText(QRect(x + 2, kLabelHeight, kColumnWidth - 4, kStripHeight - kLabelHeight), Qt::AlignCenter,
                         values[i]);
    }
    return image;
}

} // namespace NereusSDR::DeviceOutput
