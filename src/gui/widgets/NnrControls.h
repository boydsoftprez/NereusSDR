// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original WDSP 2.10 NNR controls.
// 2026-09-23: R-R3-40 runtime step-back notice and "Try again" action, by
// J.J. Boyd (KG4VCF), with Anthropic Claude Code assistance.
#pragma once

#include <QMetaObject>
#include <QPointer>
#include <QWidget>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QToolButton;

namespace NereusSDR {

class RadioModel;
class SliceModel;

class NnrControls final : public QWidget {
    Q_OBJECT

public:
    enum class Presentation { Compact, Full };

    explicit NnrControls(RadioModel* radio, SliceModel* slice,
                         Presentation presentation, QWidget* parent = nullptr);

    void bindSlice(SliceModel* slice);
    SliceModel* boundSlice() const noexcept { return m_slice.data(); }
    int boundSliceId() const noexcept { return m_sliceId; }
    bool bindingValid() const noexcept { return !m_slice.isNull(); }

public slots:
    void refresh();
    void invalidateBinding();

signals:
    void openModelsRequested(int sliceId);
    void openMoreSettingsRequested(int sliceId);
    void bindingInvalidated();

private:
    void buildUi(Presentation presentation);
    void connectSlice();
    void setInteractive(bool enabled);

    QPointer<RadioModel> m_radio;
    QPointer<SliceModel> m_slice;
    int m_sliceId{-1};
    QVector<QMetaObject::Connection> m_sliceConnections;

    QComboBox* m_model{nullptr};
    QWidget* m_limitRow{nullptr};
    QLabel* m_limitNotice{nullptr};
    QPushButton* m_tryAgain{nullptr};
    // R-R3-40: set while one model pick's signals are being handled.
    bool m_modelPickHandled{false};
    QDoubleSpinBox* m_maskFloor{nullptr};
    QSlider* m_maskFloorSlider{nullptr};
    QComboBox* m_position{nullptr};
    QDoubleSpinBox* m_alpha{nullptr};
    QDoubleSpinBox* m_alphaKnee{nullptr};
    QDoubleSpinBox* m_tau{nullptr};
    QDoubleSpinBox* m_maxGain{nullptr};
    QDoubleSpinBox* m_attack{nullptr};
    QDoubleSpinBox* m_release{nullptr};
    QWidget* m_advanced{nullptr};
    QToolButton* m_advancedToggle{nullptr};

    QLabel* m_sessionBadge{nullptr};
    QLabel* m_runtime{nullptr};
    QLabel* m_models{nullptr};
    QLabel* m_rateLatency{nullptr};
    QLabel* m_source{nullptr};
    QLabel* m_status{nullptr};
    QComboBox* m_testMode{nullptr};
    QComboBox* m_outputMode{nullptr};
    QPushButton* m_applyDiagnostic{nullptr};
    QPushButton* m_reset{nullptr};
    QPushButton* m_modelsButton{nullptr};
    QPushButton* m_moreButton{nullptr};
};

} // namespace NereusSDR
