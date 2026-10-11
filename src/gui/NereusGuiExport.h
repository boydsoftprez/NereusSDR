// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original DLL declaration attributes.
// 2026-10-09: GUI DLL exports/imports for the classes whose signals are
// used from outside NereusSDRLib (a test's QSignalSpy, or a connect in
// another image): on Windows a signal's address taken through an
// automatic import is the import thunk's, which Qt's moc code in the DLL
// does not recognise as the signal.  Empty off Windows and for the LTO
// objects linked into the executable. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
// Qt shared-library declaration pattern:
// https://doc.qt.io/qt-6/sharedlibrary.html
#pragma once

#include <QtCore/QtGlobal>

#if !defined(Q_OS_WIN) || defined(NEREUS_GUI_STATIC)
#  define NEREUS_GUI_EXPORT
#elif defined(NereusSDRLib_EXPORTS)
#  define NEREUS_GUI_EXPORT Q_DECL_EXPORT
#else
#  define NEREUS_GUI_EXPORT Q_DECL_IMPORT
#endif
