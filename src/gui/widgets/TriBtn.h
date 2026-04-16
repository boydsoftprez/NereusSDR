#pragma once

#include <QPushButton>

namespace NereusSDR {

// TriBtn — fixed-size 22×22 button that paints a filled directional triangle.
// Ported from AetherSDR src/gui/VfoWidget.cpp:97-129.
// Used for RIT/XIT zero buttons, FM offset step controls, CW pitch steppers.
class TriBtn : public QPushButton {
    Q_OBJECT
public:
    enum Dir { Left, Right };
    explicit TriBtn(Dir dir, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    Dir m_dir;
};

} // namespace NereusSDR
