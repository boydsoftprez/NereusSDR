// src/gui/ComboStyle.h
#pragma once
#include <QComboBox>
#include "StyleConstants.h"

namespace NereusSDR {

inline void applyComboStyle(QComboBox* combo)
{
    combo->setFixedHeight(Style::kButtonH);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 3px;"
        "  padding: 2px 6px; font-size: 10px;"
        "}"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox::down-arrow { image: none; border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: %1; color: %2;"
        "  selection-background-color: %4;"
        "  border: 1px solid %3;"
        "}"
    ).arg(Style::kButtonBg, Style::kTextPrimary,
          Style::kBorder, Style::kAccent));
}

} // namespace NereusSDR
