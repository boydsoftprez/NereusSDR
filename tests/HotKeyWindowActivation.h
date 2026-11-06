// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys CI fixes.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test helper.
// Making a window the one that has the keyboard, in the hot key tests, and saying
// plainly what stood in the way when it can't be.
#pragma once

#include <QApplication>
#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <QWindow>

namespace NereusSDR::Test {

inline QString namedWidget(const QWidget* widget)
{
    return widget == nullptr
        ? QStringLiteral("(none)")
        : QStringLiteral("%1#%2").arg(QString::fromLatin1(widget->metaObject()->className()), widget->objectName());
}

// Everything that decides which window has the keyboard, for a failure message.
inline QString activationState(const QWidget* wanted)
{
    QStringList shown;
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (const QWidget* const top : tops) {
        if (top->isVisible()) {
            shown << namedWidget(top);
        }
    }
    const QWindow* const handle = wanted != nullptr ? wanted->windowHandle() : nullptr;
    const QWindow* const focusWindow = QGuiApplication::focusWindow();
    return QStringLiteral("wanted %1 (handle %2, visible %3, surface %4); active window %5; popup %6; modal %7; "
                          "focus window %8; shown: %9")
        .arg(namedWidget(wanted),
             handle != nullptr ? QStringLiteral("yes") : QStringLiteral("none"),
             handle != nullptr && handle->isVisible() ? QStringLiteral("yes") : QStringLiteral("no"),
             handle != nullptr ? QString::number(int(handle->surfaceType())) : QStringLiteral("-"),
             namedWidget(QApplication::activeWindow()),
             namedWidget(QApplication::activePopupWidget()),
             namedWidget(QApplication::activeModalWidget()),
             focusWindow != nullptr ? focusWindow->objectName() + QStringLiteral(" ")
                     + QString::fromLatin1(focusWindow->metaObject()->className())
                                    : QStringLiteral("(none)"),
             shown.join(QStringLiteral(", ")));
}

// QWindow::isActive is already true for a dialog over an active parent, so
// qWaitForWindowActive can't say the dialog has the keyboard. This can.
inline bool waitActive(QWidget* window)
{
    return QTest::qWaitFor([window] { return QApplication::activeWindow() == window; }, 5000);
}

// Asks for the keyboard, even when the window already reads as active: the offscreen
// plugin doesn't hand activation back when a popup closes. Asks again until it comes.
inline bool activateAndWait(QWidget* window)
{
    window->activateWindow();
    return QTest::qWaitFor(
        [window] {
            if (QApplication::activeWindow() == window) {
                return true;
            }
            window->activateWindow();
            return false;
        },
        5000);
}

} // namespace NereusSDR::Test
