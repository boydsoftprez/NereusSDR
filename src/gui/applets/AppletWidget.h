#pragma once
#include <QWidget>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

class QSlider;
class QPushButton;
class QFrame;
class QHBoxLayout;

namespace NereusSDR {

class RadioModel;

class AppletWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletWidget(RadioModel* model, QWidget* parent = nullptr);
    ~AppletWidget() override = default;

    virtual QString appletId() const = 0;
    virtual QString appletTitle() const = 0;
    virtual QIcon appletIcon() const;
    virtual void syncFromModel() = 0;

protected:
    // Call from subclass constructor to add the gradient title bar
    QWidget* appletTitleBar(const QString& text);

    // Create standard slider row: [Label(labelWidth) | Slider(stretch) | ValueLabel(36px)]
    QHBoxLayout* sliderRow(const QString& label, QSlider* slider,
                           QLabel* valueLabel, int labelWidth = 62);

    // Button factories with checked-state colors from StyleConstants
    QPushButton* styledButton(const QString& text, int w = -1, int h = 22);
    QPushButton* greenToggle(const QString& text, int w = -1, int h = 22);
    QPushButton* blueToggle(const QString& text, int w = -1, int h = 22);
    QPushButton* amberToggle(const QString& text, int w = -1, int h = 22);

    // Inset value display label (dark background, subtle border)
    QLabel* insetValue(const QString& text = {}, int w = 40);

    // Horizontal divider line
    QFrame* divider();

    RadioModel* m_model = nullptr;
    bool m_updatingFromModel = false;
};

} // namespace NereusSDR
