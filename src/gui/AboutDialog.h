// src/gui/AboutDialog.h
#pragma once

#include <QDialog>

namespace NereusSDR {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

private:
    void buildUI();
};

} // namespace NereusSDR
