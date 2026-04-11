#pragma once

#include <QWidget>
#include <QIcon>

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
    RadioModel* m_model = nullptr;
    bool m_updatingFromModel = false;
};

} // namespace NereusSDR
