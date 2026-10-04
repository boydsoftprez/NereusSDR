// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original DLL declaration attributes.
// 2026-10-04: shared Core exports/imports, empty for native and LTO objects.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// Qt shared-library declaration pattern:
// https://doc.qt.io/qt-6/sharedlibrary.html
#pragma once

#include <QtCore/QtGlobal>

#if !defined(Q_OS_WIN) || defined(NEREUS_CORE_STATIC)
#  define NEREUS_CORE_EXPORT
#elif defined(NereusCore_EXPORTS)
#  define NEREUS_CORE_EXPORT Q_DECL_EXPORT
#else
#  define NEREUS_CORE_EXPORT Q_DECL_IMPORT
#endif
